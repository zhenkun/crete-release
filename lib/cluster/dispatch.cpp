#include <crete/cluster/dispatch.h>
#include <crete/exception.h>
#include <crete/logger.h>

#include <boost/property_tree/ptree.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/range/irange.hpp>
#include <boost/msm/back/state_machine.hpp> // back-end
#include <boost/msm/front/state_machine_def.hpp> //front-end
#include <boost/msm/front/functor_row.hpp>
#include <boost/msm/front/euml/operator.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include <chrono>
#include <deque>

namespace bpt = boost::property_tree;
namespace bui = boost::uuids;
namespace fs = boost::filesystem;
namespace msm = boost::msm;
namespace mpl = boost::mpl;
using namespace msm::front;
using namespace msm::front::euml;

namespace crete
{
namespace cluster
{

namespace vm
{
// +--------------------------------------------------+
// + Finite State Machine                             +
// +--------------------------------------------------+

// +--------------------------------------------------+
// + Flags                                            +
// +--------------------------------------------------+
namespace flag
{
    struct tx_config {};
    struct image {};
    struct trace_rxed {};
    struct tx_test {};
    struct error_rxed {};
    struct guest_data_rxed {};
    struct status_rxed {};
    struct error {};
}

// +--------------------------------------------------+
// + Events                                           +
// +--------------------------------------------------+
struct start;
struct config;
struct poll {};
struct image;
struct trace {};
struct test;

// +--------------------------------------------------+
// + State Machine Front End                          +
// +--------------------------------------------------+
class VMNodeFSM_ : public boost::msm::front::state_machine_def<VMNodeFSM_>
{
public:
    VMNodeFSM_();

    auto node_status() -> const NodeStatus&;
    auto traces() -> const std::vector<Trace>&;
    auto errors() -> const std::deque<log::NodeError>&;
    auto pop_error() -> const log::NodeError;

    // +--------------------------------------------------+
    // + Entry & Exit                                     +
    // +--------------------------------------------------+
    template <class Event,class FSM>
    void on_entry(Event const&,FSM&)
    {
    }
    template <class Event,class FSM>
    void on_exit(Event const&,FSM&)
    {
    }

    // +--------------------------------------------------+
    // + States                                           +
    // +--------------------------------------------------+
    struct Start;
    struct TxConfig;
    struct ValidateImage;
    struct RxGuestData;
    struct GuestDataRxed;
    struct UpdateImage;
    struct Commence;
    struct RxStatus;
    struct StatusRxed;
    struct RxTrace;
    struct TxTest;
    struct TraceRxed;
    struct TestTxed;
    struct ErrorRxed;
    struct Error;

    // +--------------------------------------------------+
    // + Actions                                          +
    // +--------------------------------------------------+
    struct init;
    struct tx_config;
    struct update_image_info;
    struct update_image;
    struct rx_guest_data;
    struct commence;
    struct rx_status;
    struct rx_trace;
    struct tx_test;
    struct rx_error;

    // +--------------------------------------------------+
    // + Gaurds                                           +
    // +--------------------------------------------------+
    struct is_distributed;
    struct do_update;
    struct is_first;
    struct is_image_valid;
    struct has_trace;
    struct has_error;

    // +--------------------------------------------------+
    // + Transitions                                      +
    // +--------------------------------------------------+

    template <class FSM,class Event>
    void exception_caught(Event const&,FSM& fsm,std::exception& e)
    {
        (void)fsm;
        // TODO: transition to error state.
//        fsm.process_event(ErrorConnection());
        std::cerr << boost::diagnostic_information(e) << std::endl;
        assert(0 && "VMNodeFSM_: exception thrown from within FSM");
    }

    // Initial state of the FSM.
    using initial_state = Start;

    struct transition_table : mpl::vector
    <
    //    Start              Event              Target             Action                Guard
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<Start             ,start             ,TxConfig          ,init                 ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<TxConfig          ,config            ,ValidateImage     ,tx_config            ,And_<is_distributed,
                                                                                              do_update>      >,
      Row<TxConfig          ,config            ,RxGuestData       ,tx_config            ,And_<Not_<do_update>,
                                                                                              is_first>       >,
      Row<TxConfig          ,config            ,Commence          ,tx_config            ,And_<Not_<do_update>,
                                                                                              Not_<is_first>> >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<ValidateImage     ,image             ,UpdateImage       ,none                 ,Not_<is_image_valid> >,
      Row<ValidateImage     ,image             ,RxGuestData       ,none                 ,And_<is_image_valid,
                                                                                              is_first>       >,
      Row<ValidateImage     ,image             ,Commence          ,none                 ,And_<is_image_valid,
                                                                                              Not_<is_first>> >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<UpdateImage       ,image             ,RxGuestData       ,ActionSequence_<mpl::vector<
                                                                       update_image_info,
                                                                       update_image>>   ,is_first             >,
      Row<UpdateImage       ,image             ,Commence          ,ActionSequence_<mpl::vector<
                                                                       update_image_info,
                                                                       update_image>>   ,Not_<is_first>       >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<RxGuestData       ,poll              ,GuestDataRxed     ,rx_guest_data        ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<GuestDataRxed     ,poll              ,Commence          ,none                 ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<Commence          ,poll              ,RxStatus          ,commence             ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<RxStatus          ,poll              ,StatusRxed        ,rx_status            ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<StatusRxed        ,poll              ,RxTrace           ,none                 ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<RxTrace           ,poll              ,TxTest            ,none                 ,Not_<has_trace>      >,
      Row<RxTrace           ,poll              ,TraceRxed         ,rx_trace             ,has_trace            >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<TraceRxed         ,trace             ,TxTest            ,none                 ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<TxTest            ,test              ,TestTxed          ,ActionSequence_<mpl::vector<
                                                                       tx_test,
                                                                       rx_status>>      ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<TestTxed          ,poll              ,RxStatus          ,none                 ,Not_<has_error>      >,
      Row<TestTxed          ,poll              ,ErrorRxed         ,rx_error             ,has_error            >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<ErrorRxed         ,poll              ,RxStatus          ,none                 ,none                 >
    > {};

private:
    NodeRegistrar::Node node_;
    bool first_{false};
    std::vector<Trace> traces_;
    std::deque<log::NodeError> errors_;
    boost::optional<ImageInfo> image_info_;
    bool update_image_{false};
    bool distributed_{false};
};

struct start
{
    start(const NodeRegistrar::Node& node,
          bool first,
          bool update_image,
          bool distributed) :
        node_{node},
        first_{first},
        update_image_{update_image},
        distributed_{distributed}
    {
    }

    NodeRegistrar::Node node_;
    bool first_{false};
    bool update_image_{false};
    bool distributed_{false};
};

struct config
{
    config(const option::Dispatch& options) :
        options_(options)
    {}

    option::Dispatch options_;
};

struct image
{
    image(const fs::path& image_path) :
        image_path_(image_path)
    {}

    fs::path image_path_;
};

struct test
{
    std::vector<TestCase> tests_;
};

VMNodeFSM_::VMNodeFSM_()
{
}

//auto VMNodeFSM_::node() -> NodeRegistrar::Node&
//{
//    return node_;
//}

auto VMNodeFSM_::node_status() -> const NodeStatus&
{
    return node_->acquire()->status;
}

auto VMNodeFSM_::traces() -> const std::vector<Trace>&
{
    return traces_;
}

auto VMNodeFSM_::errors() -> const std::deque<log::NodeError>&
{
    return errors_;
}

auto VMNodeFSM_::pop_error() -> const log::NodeError
{
    auto e = errors_.front();

    errors_.pop_front();

    return e;
}

// +--------------------------------------------------+
// + States                                           +
// +--------------------------------------------------+

struct VMNodeFSM_::Start : public msm::front::state<>
{
#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: Start" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: Start" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct VMNodeFSM_::TxConfig : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::tx_config>;

#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: TxConfig" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: TxConfig" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct VMNodeFSM_::ValidateImage : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::image>;

#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: ValidateImage" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: ValidateImage" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct VMNodeFSM_::RxGuestData : public msm::front::state<>
{
#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: RxGuestData" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: RxGuestData" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct VMNodeFSM_::GuestDataRxed : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::guest_data_rxed>;

#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: GuestDataRxed" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: GuestDataRxed" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct VMNodeFSM_::UpdateImage : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::image>;

#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: UpdateImage" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: UpdateImage" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct VMNodeFSM_::Commence : public msm::front::state<>
{
#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: Commence" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: Commence" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct VMNodeFSM_::RxStatus : public msm::front::state<>
{
#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: RxStatus" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: RxStatus" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct VMNodeFSM_::StatusRxed : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::status_rxed>;

#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: StatusRxed" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: StatusRxed" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct VMNodeFSM_::RxTrace : public msm::front::state<>
{
#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: RxTrace" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: RxTrace" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct VMNodeFSM_::TxTest : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::tx_test>;

#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: TxTest" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: TxTest" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct VMNodeFSM_::TraceRxed : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::trace_rxed>;

#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: TraceRxed" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: TraceRxed" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct VMNodeFSM_::TestTxed : public msm::front::state<>
{
#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: TestTxed" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: TestTxed" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct VMNodeFSM_::ErrorRxed : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::error_rxed>;

#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: ErrorRxed" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: ErrorRxed" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct VMNodeFSM_::Error : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::error>;

#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: Error" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: Error" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

// +--------------------------------------------------+
// + Actions                                          +
// +--------------------------------------------------+

struct VMNodeFSM_::init
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const& ev, FSM& fsm, SourceState&, TargetState&) -> void
    {
        fsm.node_ = ev.node_;
        fsm.first_ = ev.first_;
        fsm.update_image_ = ev.update_image_;
        fsm.distributed_ = ev.distributed_;
    }
};

struct VMNodeFSM_::tx_config
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const& ev, FSM& fsm, SourceState&, TargetState&) -> void
    {
        transmit_config(fsm.node_,
                        ev.options_);
    }
};

struct VMNodeFSM_::update_image_info
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const& ev, FSM& fsm, SourceState&, TargetState&) -> void
    {
        transmit_image_info(fsm.node_,
                            ImageInfo{ev.image_path_});
    }
};

struct VMNodeFSM_::update_image
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const& ev, FSM& fsm, SourceState&, TargetState&) -> void
    {
        if(!fs::exists(ev.image_path_))
        {
            BOOST_THROW_EXCEPTION(Exception{} << err::file_missing{ev.image_path_.string()});
        }

        auto image = OSImage{};
        auto tared_path = fs::path{ev.image_path_}.replace_extension(".tar.gz");
        auto pkinfo = PacketInfo{0,0,0};

        if(fs::exists(tared_path))
        {
            fs::remove(tared_path);
        }

        image = from_image_file(ev.image_path_);

        auto lock = fsm.node_->acquire();

        pkinfo.id = lock->status.id;
        pkinfo.type = packet_type::cluster_image;

        write_serialized_binary(lock->server,
                                pkinfo,
                                image);
    }
};

struct VMNodeFSM_::rx_guest_data
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM&, SourceState&, TargetState&) -> void
    {
        // TODO: possibly compare proc-maps, to verify they are all consistent? Get elf-info from first, etc.
    }
};

struct VMNodeFSM_::commence
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> void
    {
        transmit_commencement(fsm.node_);
    }
};

struct VMNodeFSM_::rx_status
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> void
    {
        cluster::poll(fsm.node_);
    }
};

struct VMNodeFSM_::rx_trace
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> void
    {
        fsm.traces_ = receive_traces(fsm.node_);
    }
};

struct VMNodeFSM_::tx_test
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const& ev, FSM& fsm, SourceState&, TargetState&) -> void
    {
        if(ev.tests_.empty())
        {
            return;
        }

        transmit_tests(fsm.node_,
                       ev.tests_);
    }
};

struct VMNodeFSM_::rx_error
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> void
    {
        auto v = receive_errors(fsm.node_);

        fsm.errors_ = std::deque<log::NodeError>{v.begin(),
                                                 v.end()};
    }
};

// +--------------------------------------------------+
// + Gaurds                                           +
// +--------------------------------------------------+

struct VMNodeFSM_::is_distributed
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> bool
    {
        return fsm.distributed_;
    }
};

struct VMNodeFSM_::do_update
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> bool
    {
        return fsm.update_image_;
    }
};

struct VMNodeFSM_::is_first
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&,TargetState&) -> bool
    {
        return fsm.first_;
    }
};

struct VMNodeFSM_::is_image_valid
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const& ev, FSM& fsm, SourceState&,TargetState&) -> bool
    {
        if(!fsm.image_info_)
        {
            fsm.image_info_ = receive_image_info(fsm.node_);
        }

        auto dispatch_image_info = ImageInfo{ev.image_path_};

        // Node has no image.
        if(fsm.image_info_->file_name_.empty())
        {
            return false;
        }

        // Node image needs upating
        if(dispatch_image_info != *fsm.image_info_)
        {
            return false;
        }

        return true;
    }
};

struct VMNodeFSM_::has_trace
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&,TargetState&) -> bool
    {
        return fsm.node_->acquire()->status.trace_count > 0;
    }
};

struct VMNodeFSM_::has_error
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&,TargetState&) -> bool
    {
        return fsm.node_->acquire()->status.error_count > 0;
    }
};

// Normally would just: "using NodeFSM = boost::msm::back::state_machine<VMNodeFSM_>;",
// but needed a way to hide the impl. in source file, so as to avoid namespace troubles.
// This seems to work.
class NodeFSM : public boost::msm::back::state_machine<VMNodeFSM_>
{
};

} // namespace vm

namespace svm
{
// +--------------------------------------------------+
// + Finite State Machine                             +
// +--------------------------------------------------+

// +--------------------------------------------------+
// + Flags                                            +
// +--------------------------------------------------+
namespace flag
{
    using status_rxed = vm::flag::status_rxed;
    struct test_rxed {};
    struct tx_trace {};
    struct error {};
}

// +--------------------------------------------------+
// + Events                                           +
// +--------------------------------------------------+
struct start;
using config = vm::config;
struct poll {};
struct trace;
struct test {};

// +--------------------------------------------------+
// + State Machine Front End                          +
// +--------------------------------------------------+
class SVMNodeFSM_ : public boost::msm::front::state_machine_def<SVMNodeFSM_>
{
private:
    NodeRegistrar::Node node_;
    std::vector<TestCase> tests_;
    std::deque<log::NodeError> errors_;

    friend class vm::VMNodeFSM_; // Allow reuse of VMNode's actions/guards with private members.

public:
    SVMNodeFSM_();

//    auto node() -> const NodeRegistrar::Node&;
    auto node_status() -> const NodeStatus&;
    auto tests() -> const std::vector<TestCase>&;
    auto errors() -> const std::deque<log::NodeError>&;
    auto pop_error() -> const log::NodeError;

    // +--------------------------------------------------+
    // + Entry & Exit                                     +
    // +--------------------------------------------------+
    template <class Event,class FSM>
    void on_entry(Event const&,FSM&)
    {
    }
    template <class Event,class FSM>
    void on_exit(Event const&,FSM&)
    {
    }

    // +--------------------------------------------------+
    // + States                                           +
    // +--------------------------------------------------+
    struct Start;
    using TxConfig = vm::NodeFSM::TxConfig;
    struct Commence;
    using  RxStatus = vm::NodeFSM::RxStatus;
    using  StatusRxed = vm::NodeFSM::StatusRxed;
    struct TxTrace;
    struct TraceTxed;
    struct RxTest;
    struct TestRxed;
    using ErrorRxed = vm::NodeFSM::ErrorRxed;
    struct Error;

    // +--------------------------------------------------+
    // + Actions                                          +
    // +--------------------------------------------------+
    struct init;
    using tx_config = vm::NodeFSM::tx_config;
    struct commence;
    struct tx_trace;
    struct rx_test;
    using rx_status = vm::NodeFSM::rx_status;
    using rx_error = vm::NodeFSM::rx_error;

    // +--------------------------------------------------+
    // + Gaurds                                           +
    // +--------------------------------------------------+
    struct has_tests;
    using has_error = vm::NodeFSM::has_error;

    // +--------------------------------------------------+
    // + Transitions                                      +
    // +--------------------------------------------------+

    template <class FSM,class Event>
    void exception_caught(Event const&,FSM& fsm,std::exception& e)
    {
        (void)fsm;
        // TODO: transition to error state.
//        fsm.process_event(ErrorConnection());
        std::cerr << boost::diagnostic_information(e) << std::endl;
        assert(0 && "VMNodeFSM_: exception thrown from within FSM");
    }

    // Initial state of the FSM.
    using initial_state = Start;

    struct transition_table : mpl::vector
    <
    //    Start              Event              Target             Action                Guard
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<Start             ,start             ,TxConfig          ,init                 ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<TxConfig          ,config            ,Commence          ,tx_config            ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<Commence          ,poll              ,RxStatus          ,commence             ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<RxStatus          ,poll              ,StatusRxed        ,rx_status            ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<StatusRxed        ,poll              ,TxTrace           ,none                 ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<TxTrace           ,trace             ,TraceTxed         ,ActionSequence_<mpl::vector<
                                                                       tx_trace,
                                                                       rx_status>>      ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<TraceTxed         ,poll              ,RxTest            ,none                 ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<RxTest            ,poll              ,RxStatus          ,none                 ,And_<Not_<has_tests>,
                                                                                              Not_<has_error>>>,
      Row<RxTest            ,poll              ,ErrorRxed         ,rx_error             ,And_<Not_<has_tests>,
                                                                                              has_error>      >,
      Row<RxTest            ,poll              ,TestRxed          ,rx_test              ,has_tests            >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<TestRxed          ,test              ,RxStatus          ,none                 ,Not_<has_error>      >,
      Row<TestRxed          ,test              ,ErrorRxed         ,rx_error             ,has_error            >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<ErrorRxed         ,poll              ,RxStatus          ,none                 ,none                 >
    > {};
};

struct start
{
    start(NodeRegistrar::Node& node) :
        node_{node}
    {}

    NodeRegistrar::Node node_;
};

struct trace
{
    std::vector<Trace> traces_;
};

SVMNodeFSM_::SVMNodeFSM_()
{
}

auto SVMNodeFSM_::node_status() -> const NodeStatus&
{
    return node_->acquire()->status;
}

auto SVMNodeFSM_::tests() -> const std::vector<TestCase>&
{
    return tests_;
}

auto SVMNodeFSM_::errors() -> const std::deque<log::NodeError>&
{
    return errors_;
}

auto SVMNodeFSM_::pop_error() -> const log::NodeError
{
    auto e = errors_.front();

    errors_.pop_front();

    return e;
}

// +--------------------------------------------------+
// + States                                           +
// +--------------------------------------------------+

struct SVMNodeFSM_::Start : public msm::front::state<>
{
#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: Start" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: Start" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct SVMNodeFSM_::Commence : public msm::front::state<>
{
#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: Commence" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: Commence" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct SVMNodeFSM_::TxTrace : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::tx_trace>;

#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: TxTrace" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: TxTrace" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct SVMNodeFSM_::TraceTxed : public msm::front::state<>
{
#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: TraceTxed" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: TraceTxed" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct SVMNodeFSM_::RxTest : public msm::front::state<>
{
#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: RxTest" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: RxTest" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct SVMNodeFSM_::TestRxed : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::test_rxed>;

#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: TestRxed" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: TestRxed" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct SVMNodeFSM_::Error : public msm::front::state<>
{
    using flag_list = mpl::vector1<flag::error>;

#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const& ,FSM&) {std::cout << "entering: Error" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: Error" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

// +--------------------------------------------------+
// + Actions                                          +
// +--------------------------------------------------+

struct SVMNodeFSM_::init
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const& ev, FSM& fsm, SourceState&, TargetState&) -> void
    {
        fsm.node_ = ev.node_;
    }
};

struct SVMNodeFSM_::commence
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> void
    {
        transmit_commencement(fsm.node_);
    }
};

struct SVMNodeFSM_::tx_trace
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const& ev, FSM& fsm, SourceState&, TargetState&) -> void
    {
        if(ev.traces_.empty())
        {
            return;
        }

        transmit_traces(fsm.node_,
                        ev.traces_);
    }
};

struct SVMNodeFSM_::rx_test
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> void
    {
        fsm.tests_ = receive_tests(fsm.node_);
    }
};

// +--------------------------------------------------+
// + Gaurds                                           +
// +--------------------------------------------------+

struct SVMNodeFSM_::has_tests
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> bool
    {
        return fsm.node_->acquire()->status.test_case_count > 0;
    }
};

// Normally would just: "using NodeFSM = boost::msm::back::state_machine<SVMNodeFSM_>;",
// but needed a way to hide the impl. in source file, so as to avoid namespace troubles.
// This seems to work.
class NodeFSM : public boost::msm::back::state_machine<SVMNodeFSM_>
{
};

} // namespace svm

namespace fsm
{

// +--------------------------------------------------+
// + Finite State Machine                             +
// +--------------------------------------------------+

// +--------------------------------------------------+
// + Flags                                            +
// +--------------------------------------------------+
namespace flag
{
struct terminated {};
}

// +--------------------------------------------------+
// + Events                                           +
// +--------------------------------------------------+
struct start;
struct poll {};

// +--------------------------------------------------+
// + State Machine Front End                          +
// +--------------------------------------------------+
class DispatchFSM_ : public boost::msm::front::state_machine_def<DispatchFSM_>
{
public:
    using VMNodeFSM = std::shared_ptr<vm::NodeFSM>;
    using VMNodeFSMs = std::vector<VMNodeFSM>;
    using SVMNodeFSM = std::shared_ptr<svm::NodeFSM>;
    using SVMNodeFSMs = std::vector<SVMNodeFSM>;

public:
    DispatchFSM_();
    ~DispatchFSM_();

    auto to_trace_pool(const Trace& trace) -> void;
    auto to_trace_pool(const std::vector<Trace>& trace) -> void;
    auto next_trace() -> boost::optional<Trace>;
    auto next_test() -> boost::optional<TestCase>;
    auto node_registrar() -> AtomicGuard<NodeRegistrar>&;
    auto display_status(std::ostream& os) -> void;
    auto write_statistics() -> void;
    auto test_pool() -> TestPool&;
    auto trace_pool() -> TracePool&;
    auto set_up_root_dir() -> void;
    auto launch_node_registrar(Port master) -> void;
    auto elapsed_time() -> uint64_t;
    auto are_node_queues_empty() -> bool;
    auto are_all_queues_empty() -> bool;
    auto are_nodes_inactive() -> bool;
    auto is_converged() -> bool;
    auto write_target_log(const log::NodeError& ne,
                          const fs::path& subdir) -> void;

    // +--------------------------------------------------+
    // + Entry & Exit                                     +
    // +--------------------------------------------------+
    template <class Event,class FSM>
    void on_entry(Event const&,FSM&)
    {
    }
    template <class Event,class FSM>
    void on_exit(Event const&,FSM&)
    {
    }

    // +--------------------------------------------------+
    // + States                                           +
    // +--------------------------------------------------+
    struct Start;
    struct SpecCheck;
    struct NextTarget;
    struct Dispatch;
    struct Terminate;
    struct Terminated;

    // +--------------------------------------------------+
    // + Actions                                          +
    // +--------------------------------------------------+
    struct init;
    struct reset;
    struct assign_next_target;
    struct dispatch;
    struct finish;
    struct next_target_clean;
    struct terminate;

    // +--------------------------------------------------+
    // + Gaurds                                           +
    // +--------------------------------------------------+
    struct is_first;
    struct is_dev_mode;
    struct have_next_target;
    struct is_target_expired;

    // +--------------------------------------------------+
    // + Transitions                                      +
    // +--------------------------------------------------+

    template <class FSM,class Event>
    void exception_caught(Event const&,FSM& fsm,std::exception& e)
    {
        (void)fsm;
        // TODO: transition to error state.
//        fsm.process_event(ErrorConnection());
        std::cerr << boost::diagnostic_information(e) << std::endl;
        assert(0 && "VMNodeFSM_: exception thrown from within FSM");
    }

    // Initial state of the FSM.
    using initial_state = Start;

    struct transition_table : mpl::vector
    <
    //    Start              Event              Target             Action                Guard
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<Start             ,start             ,SpecCheck         ,init                 ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<SpecCheck         ,poll              ,NextTarget        ,none                 ,And_<Not_<is_dev_mode>,
                                                                                              Or_<is_first,
                                                                                                  And_<is_target_expired,
                                                                                                       have_next_target>>>>,
      Row<SpecCheck         ,poll              ,Dispatch          ,none                 ,Or_<is_dev_mode,
                                                                                             And_<Not_<is_first>,
                                                                                                  Not_<is_target_expired>>>>,
      Row<SpecCheck         ,poll              ,Terminate         ,next_target_clean    ,And_<is_target_expired,
                                                                                              Not_<have_next_target>>>,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<NextTarget        ,poll              ,Dispatch          ,ActionSequence_<mpl::vector<
                                                                       finish,
                                                                       next_target_clean,
                                                                       reset,
                                                                       assign_next_target>>
                                                                                        ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<Dispatch          ,poll              ,SpecCheck         ,dispatch             ,none                 >,
    //   +------------------+------------------+------------------+---------------------+------------------+
      Row<Terminate         ,poll              ,Terminated        ,ActionSequence_<mpl::vector<
                                                                       finish,
                                                                       terminate>>      ,none                 >
    > {};

private:
    option::Dispatch options_;
    AtomicGuard<NodeRegistrar> node_registrar_;
    boost::thread node_registrar_driver_thread_;
    boost::filesystem::path root_{make_dispatch_root()};
    TestPool test_pool_{root_};
//    TracePool trace_pool_{option::Dispatch{}, "weighted"};
    TracePool trace_pool_{option::Dispatch{}, "fifo"};
    AtomicGuard<VMNodeFSMs> vm_node_fsms_;
    AtomicGuard<SVMNodeFSMs> svm_node_fsms_;
    Port master_port_;
    crete::log::Logger exception_log_;
    crete::log::Logger node_error_log_;

    std::chrono::time_point<std::chrono::system_clock> start_time_ = std::chrono::system_clock::now();
    bool first_{true};
    std::deque<std::string> next_target_queue_;
    std::string target_;
};

struct start
{
    start(Port master_port,
          const option::Dispatch& options)
        : options_{options}
        , master_port_{master_port} {}

    const option::Dispatch& options_;
    Port master_port_;
};

// +--------------------------------------------------+
// + States                                           +
// +--------------------------------------------------+

struct DispatchFSM_::Start : public msm::front::state<>
{
#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const&,FSM&) {std::cout << "entering: Start" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: Start" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct DispatchFSM_::SpecCheck : public msm::front::state<>
{
#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const&,FSM&) {std::cout << "entering: SpecCheck" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: SpecCheck" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct DispatchFSM_::NextTarget : public msm::front::state<>
{
#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const&,FSM&) {std::cout << "entering: NextTarget" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: NextTarget" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct DispatchFSM_::Dispatch : public msm::front::state<>
{
#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const&,FSM&) {std::cout << "entering: Dispatch" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: Dispatch" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct DispatchFSM_::Terminate : public msm::front::state<>
{
#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const&,FSM&) {std::cout << "entering: Terminate" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: Terminate" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

struct DispatchFSM_::Terminated : public msm::front::terminate_state<>
{
    using flag_list = mpl::vector1<flag::terminated>;

#if defined(CRETE_DEBUG)
    template <class Event,class FSM>
    void on_entry(Event const&,FSM&) {std::cout << "entering: Terminated" << std::endl;}
    template <class Event,class FSM>
    void on_exit(Event const&,FSM& ) {std::cout << "leaving: Terminated" << std::endl;}
#endif // defined(CRETE_DEBUG)
};

// +--------------------------------------------------+
// + Gaurds                                           +
// +--------------------------------------------------+

struct DispatchFSM_::is_first
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> bool
    {
        return fsm.first_;
    }
};

struct DispatchFSM_::is_dev_mode
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> bool
    {
        return !fsm.options_.mode.distributed;
    }
};

struct DispatchFSM_::have_next_target
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> bool
    {
        return !fsm.next_target_queue_.empty();
    }
};

struct DispatchFSM_::is_target_expired
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> bool
    {
        auto elapsed_time_count = fsm.elapsed_time();

        assert(elapsed_time_count >= 0);

        auto converged = fsm.is_converged();
        auto trace_exceeded = fsm.trace_pool_.count_all_unique() >= fsm.options_.test.interval.trace;
        auto tc_exceeded = fsm.test_pool_.count_all() >= fsm.options_.test.interval.tc;
        auto time_exceeded = elapsed_time_count >= fsm.options_.test.interval.time;

        return
                   converged
                || trace_exceeded
                || tc_exceeded
                || time_exceeded
                ;
    }
};

// +--------------------------------------------------+
// + Actions                                          +
// +--------------------------------------------------+

struct DispatchFSM_::init
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const& ev, FSM& fsm, SourceState&, TargetState&) -> void
    {
        fsm.exception_log_.add_sink(fsm.root_ / log_dir_name / exception_log_file_name);
        fsm.exception_log_.auto_flush(true);
        fsm.node_error_log_.add_sink(fsm.root_ / log_dir_name / dispatch_node_error_log_file_name);
        fsm.node_error_log_.auto_flush(true);

        fsm.master_port_ = ev.master_port_;
        fsm.options_ = ev.options_;
        fsm.next_target_queue_ = std::deque<std::string>{fsm.options_.test.items.begin(),
                                                         fsm.options_.test.items.end()};

//        fsm.trace_pool_ = TracePool{fsm.options_, "weighted"}; // TODO: get strategy from guest config.
        fsm.trace_pool_ = TracePool{fsm.options_, "fifo"}; // TODO: get strategy from guest config.

        fsm.launch_node_registrar(fsm.master_port_);

        if(!fsm.options_.mode.distributed) // TODO: should be encoded into FSM.
        {
            fsm.set_up_root_dir();
        }
    }
};

struct DispatchFSM_::reset
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> void
    {
        fsm.set_up_root_dir();

        fsm.test_pool_ = TestPool{fsm.root_};
//        fsm.trace_pool_ = TracePool{fsm.options_, "weighted"}; // TODO: get strategy from guest config.
        fsm.trace_pool_ = TracePool{fsm.options_, "fifo"}; // TODO: get strategy from guest config.

        fsm.vm_node_fsms_.acquire()->clear();
        fsm.svm_node_fsms_.acquire()->clear();

        fsm.start_time_ = std::chrono::system_clock::now();

        {
            auto lock = fsm.node_registrar_.acquire();

            for(auto& node : lock->nodes())
            {
                {
                    auto nl = node->acquire();
                    auto pkinfo = PacketInfo{0,0,0};
                    pkinfo.id = nl->status.id;
                    pkinfo.type = packet_type::cluster_reset;

                    nl->server.write(pkinfo);
                }

                register_node_fsm(node,
                                  fsm.options_,
                                  fsm.vm_node_fsms_,
                                  fsm.svm_node_fsms_);
            }

        }
    }
};

struct DispatchFSM_::assign_next_target
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> void
    {
        {
            auto lock = fsm.node_registrar_.acquire();

            for(auto& node : lock->nodes())
            {
                auto nl = node->acquire();

                if(nl->type != packet_type::cluster_request_vm_node)
                {
                    continue;
                }

                auto pkinfo = PacketInfo{0,0,0};
                pkinfo.id = nl->status.id;
                pkinfo.type = packet_type::cluster_next_target;

                auto& queue = fsm.next_target_queue_;

                write_serialized_binary(nl->server,
                                        pkinfo,
                                        queue.front());

                fsm.target_ = queue.front();

                queue.pop_front();
            }
        }
    }
};

struct DispatchFSM_::dispatch
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> void
    {
        {
            auto vmns_lock = fsm.vm_node_fsms_.acquire();

            std::for_each(vmns_lock->begin(),
                          vmns_lock->end(),
                          [&] (VMNodeFSM& nfsm)
            {


                if(nfsm->is_flag_active<vm::flag::trace_rxed>())
                {
                    fsm.to_trace_pool(nfsm->traces());

                    nfsm->process_event(vm::trace{});
                }
                else if(nfsm->is_flag_active<vm::flag::tx_test>())
                {
                    auto tests = std::vector<TestCase>{};
                    auto tc_count = nfsm->node_status().test_case_count;

                    while(tc_count < (1*vm_test_multiplier)) // TODO: should be num_vm_insts*vm_test_multiplier. Also, should verify bandwidth, though I doubt this would be a problem.
                    {
                        auto next = fsm.next_test();

                        if(!next)
                            break;

                        tests.emplace_back(*next);

                        ++tc_count;
                    }

                    nfsm->process_event(vm::test{tests});
                }
                else if(nfsm->is_flag_active<vm::flag::error_rxed>())
                {
                    while(!nfsm->errors().empty())
                    {
                        auto err = nfsm->pop_error();

                        fsm.write_target_log(err, dispatch_log_vm_dir_name);
                        fsm.node_error_log_ << "Target: " << fsm.target_ << "\n"
                                            <<  err.log << "\n";
                    }

                    nfsm->process_event(vm::poll{});
                }
                else if(nfsm->is_flag_active<vm::flag::tx_config>())
                {
                    nfsm->process_event(vm::config{fsm.options_});
                }
                else if(nfsm->is_flag_active<vm::flag::image>())
                {
                    nfsm->process_event(vm::image{fsm.options_.vm.image.path});
                }
                else
                {
                    nfsm->process_event(vm::poll{});
                }
            });
        }

        {
            auto svmns_lock = fsm.svm_node_fsms_.acquire();

            std::for_each(svmns_lock->begin(),
                          svmns_lock->end(),
                          [&] (SVMNodeFSM& nfsm)
            {

                if(nfsm->is_flag_active<svm::flag::test_rxed>())
                {
                    fsm.test_pool_.insert(nfsm->tests());

                    nfsm->process_event(svm::test{});
                }
                else if(nfsm->is_flag_active<svm::flag::tx_trace>())
                {
                    auto traces = std::vector<Trace>{};
                    auto trace_count = nfsm->node_status().trace_count;

                    while(trace_count < (1*vm_trace_multiplier)) // TODO: should be num_vm_insts*vm_trace_multiplier. Also, should verify bandwidth, though I doubt this would be a problem.
                    {
                        try // TODO: I don't like it, but the trace could fail somehow (bug) and we need to continue testing. Have a better way?
                        {   // Cont: what seems to be causing the bug is that a supergraph is found which in turn causes a callback to call and remove it from the trace pool.
                            // Cont: This, then, seems to cause the problem. Some reference to the trace is removed, while another is perserved. When lookup is done based on the preserved, the removed raises an exception.
                            auto next = fsm.next_trace();

                            if(!next)
                                break;

                            traces.emplace_back(*next);

                            ++trace_count;
                        }
                        catch(std::exception& e)
                        {
                            fsm.exception_log_
                                    << boost::diagnostic_information(e)
                                    << "\n"
                                    << __FILE__
                                    << ": "
                                    << __LINE__
                                    << "\n";
                        }
                    }

                    nfsm->process_event(svm::trace{traces});
                }
                else if(nfsm->is_flag_active<vm::flag::error_rxed>())
                {
                    while(!nfsm->errors().empty())
                    {
                        auto err = nfsm->pop_error();

                        fsm.write_target_log(err, dispatch_log_svm_dir_name);
                        fsm.node_error_log_ << "Target: " << fsm.target_ << "\n"
                                            <<  err.log << "\n";
                    }

                    nfsm->process_event(svm::poll{});
                }
                else if(nfsm->is_flag_active<vm::flag::tx_config>())
                {
                    nfsm->process_event(vm::config{fsm.options_});
                }
                else
                {
                    nfsm->process_event(svm::poll{});
                }
            });
        }

        fsm.first_ = false;

        fsm.display_status(std::cout);
        fsm.write_statistics();
    }
};

struct DispatchFSM_::next_target_clean
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> void
    {
        // No need to store expensive traces once we're done testing.
        fs::remove_all(fsm.root_ / dispatch_trace_dir_name);
    }
};

struct DispatchFSM_::terminate
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM&, SourceState&, TargetState&) -> void
    {
        // TODO: handle termination?
    }
};

struct DispatchFSM_::finish
{
    template <class EVT,class FSM,class SourceState,class TargetState>
    auto operator()(EVT const&, FSM& fsm, SourceState&, TargetState&) -> void
    {
        auto p = fsm.root_ / log_dir_name;

        if(!fs::exists(p)) // TODO: encode as part of transition table. We only want to write the 'finish' file for existing tests, not the first time NextTest entered.
        {
            return;
        }

        p /= dispatch_log_finish_file_name;

        fs::ofstream ofs{p};

        if(!ofs.good())
        {
            BOOST_THROW_EXCEPTION(Exception{} << err::file_open_failed{p.string()});
        }

        fsm.display_status(ofs);
    }
};

DispatchFSM_::DispatchFSM_()
{
}

DispatchFSM_::~DispatchFSM_()
{
    if(node_registrar_driver_thread_.joinable())
    {
        node_registrar_driver_thread_.join();
    }
}

auto DispatchFSM_::to_trace_pool(const Trace& trace) -> void
{
    auto p = root_ / dispatch_trace_dir_name / bui::to_string(trace.uuid_);

    to_file(trace,
            p);

    if(!fs::exists(p))
    {
        BOOST_THROW_EXCEPTION(Exception{} << err::file_missing{p.string()});
    }

    trace_pool_.insert(p);
}

auto DispatchFSM_::to_trace_pool(const std::vector<Trace>& traces) -> void
{
    for(const auto& trace : traces)
    {
        to_trace_pool(trace);
    }
}

auto DispatchFSM_::next_trace() -> boost::optional<Trace>
{
    auto p = trace_pool_.next();

    if(!p)
    {
        return boost::optional<Trace>{};
    }

    auto trace = from_trace_file(*p);

    return boost::optional<Trace>{trace};
}

auto DispatchFSM_::next_test() -> boost::optional<TestCase>
{
    return test_pool_.next();
}

auto DispatchFSM_::launch_node_registrar(Port master) -> void
{
    node_registrar_driver_thread_ =
            boost::thread{NodeRegistrarDriver{master,
                          node_registrar_,
                          [this] (NodeRegistrar::Node& node) {
        register_node_fsm(node,
                          options_,
                          vm_node_fsms_,
                          svm_node_fsms_);
    }}};
}

auto DispatchFSM_::elapsed_time() -> uint64_t
{
    using namespace std::chrono;

    auto current_time = system_clock::now();

    return duration_cast<seconds>(current_time - start_time_).count();
}

auto DispatchFSM_::are_node_queues_empty() -> bool
{
    auto lock = node_registrar_.acquire();
    auto& nodes = lock->nodes();

    auto nonempty = true;

    for(const auto& n : nodes)
    {
        auto nl = n->acquire();

        const auto& st = nl->status;

        if(st.test_case_count != 0 || st.trace_count != 0)
        {
            nonempty = false;

            break;
        }
    }

    return nonempty;
}

auto DispatchFSM_::are_all_queues_empty() -> bool
{
    return
            are_node_queues_empty()
         && test_pool_.count_next() == 0
         && trace_pool_.count_next() == 0;
}

auto DispatchFSM_::are_nodes_inactive() -> bool
{
    auto lock = node_registrar_.acquire();
    auto& nodes = lock->nodes();

    auto inactive = true;

    for(const auto& n : nodes)
    {
        auto nl = n->acquire();

        const auto& st = nl->status;

        if(st.active)
        {
            inactive = false;

            break;
        }
    }

    return inactive;
}

auto DispatchFSM_::is_converged() -> bool
{
    return
               are_all_queues_empty()
            && are_nodes_inactive();
}

auto DispatchFSM_::write_target_log(const log::NodeError& ne,
                                    const fs::path& subdir) -> void
{
    auto p = root_ / log_dir_name / subdir;

    {
        auto i = 1u;

        while(fs::exists(p / std::to_string(i)))
        {
            ++i;
        }

        p /= std::to_string(i);
    }

    fs::ofstream ofs{p};

    if(!ofs.good())
    {
        BOOST_THROW_EXCEPTION(Exception{} << err::file_open_failed{p.string()});
    }

    ofs << ne.log;
}


auto DispatchFSM_::test_pool() -> TestPool&
{
    return test_pool_;
}

auto DispatchFSM_::trace_pool() -> TracePool&
{
    return trace_pool_;
}

auto DispatchFSM_::set_up_root_dir() -> void
{
    auto timestamp_root = root_.filename();

    if(options_.mode.distributed)
    {
        auto target = fs::path{next_target_queue_.front()}.filename();

        if(root_.parent_path().filename() == dispatch_root_dir_name)
        {
            timestamp_root = root_.filename();
            root_ /= target;
        }
        else
        {
            timestamp_root = root_.parent_path().filename();
            root_ = root_.parent_path() / target;
        }
    }

    auto create_dirs = [](const fs::path& root)
    {
        return [root](const std::vector<std::string>& v)
        {
            for(const auto& e : v)
            {
                auto d = root / e;
                if(!fs::create_directories(d))
                {
                    BOOST_THROW_EXCEPTION(Exception{} << err::file_create{d.string()});
                }
            }
        };
    };

    if(!fs::exists(root_))
    {
        auto create_root_dirs = create_dirs(root_);
        auto create_log_dirs = create_dirs(root_ / log_dir_name);

        create_root_dirs({dispatch_trace_dir_name,
                          dispatch_test_case_dir_name,
                          dispatch_profile_dir_name});
        create_log_dirs({dispatch_log_vm_dir_name,
                         dispatch_log_svm_dir_name});
    }

    auto last_symlink = fs::path{dispatch_root_dir_name} / dispatch_last_root_symlink;

    fs::remove(last_symlink);
    fs::create_symlink(timestamp_root,
                       last_symlink);
}

auto DispatchFSM_::display_status(std::ostream& os) -> void
{
    using namespace std;

    system("clear");

    os << setw(12) << "time (s)"
         << "|"
         << setw(12) << "tests left"
         << "|"
         << setw(12) << "traces left"
         << "|";

    {
        auto count = 1u;
        auto lock = node_registrar_.acquire();
        for(const auto& node : lock->nodes())
        {
            auto tt = std::string{};

            tt += to_string(count++);

            if(node->acquire()->type == packet_type::cluster_request_vm_node)
                tt += "-[vm]";
            else
                tt += "-[svm]";

            tt += " tc/tr";

            os << setw(14) << tt
                 << "|";
        }
    }

    os << endl;

    auto etime = elapsed_time();
    auto test = to_string(test_pool_.count_next()) +
                 "/" +
                 to_string(test_pool_.count_all());
    auto trace = to_string(trace_pool_.count_next()) +
                 "/" +
                 to_string(trace_pool_.count_all_unique());

    os << setw(12) << etime
         << "|"
         << setw(12) << test
         << "|"
         << setw(12) << trace
         << "|";

    {
        auto lock = node_registrar_.acquire();
        for(const auto& node : lock->nodes())
        {
            auto tt = std::string{};
            auto lock = node->acquire();

            tt += to_string(lock->status.test_case_count) +
                  "/" +
                  to_string(lock->status.trace_count);
            os << setw(14) << tt
                 << "|";
        }
    }

    os << endl;
}

auto DispatchFSM_::write_statistics() -> void
{
    static auto prev_time = decltype(elapsed_time()){0};
    static auto print_pg = true;
    auto time = elapsed_time();
    auto dir = root_ / dispatch_profile_dir_name;

    if(time - prev_time >= options_.profile.interval)
    {
        prev_time = time;
    }
    else
    {
        return;
    }

    if(print_pg)
    {
        print_pg = false;

        fs::ofstream ofs{dir / "stat.pg"};

        ofs << R"(#!/usr/bin/gnuplot
               reset
               set terminal png

               set title "Test cases and traces per second"
               set grid
               set key reverse Left outside
               set style data linespoints

               set ylabel "tcs/traces"

               set xlabel "seconds"

               plot "stat.dat" using 1:2 title "tc remaining", \
               "" using 1:3 title "tc total", \
               "" using 1:4 title "trace remaining", \
               "" using 1:5 title "trace total"
               #)";
    }

    fs::ofstream ofs{dir / "stat.dat"
                    ,std::ios_base::app};

    auto tc_left = test_pool_.count_next();
    auto tc_total = test_pool_.count_all();
    auto trace_left = trace_pool_.count_next();
    auto trace_total = trace_pool_.count_all_unique();

    ofs << time
        << " "
        << tc_left
        << " "
        << tc_total
        << " "
        << trace_left
        << " "
        << trace_total
        << "\n";
}

auto DispatchFSM_::node_registrar() -> AtomicGuard<NodeRegistrar>&
{
    return node_registrar_;
}

class DispatchFSM : public boost::msm::back::state_machine<DispatchFSM_>
{
};

} // namespace fsm


// +--------------------------------------------------+
// + Dispatch                                         +
// +--------------------------------------------------+

Dispatch::Dispatch(Port master,
                   const option::Dispatch& options)
    : dispatch_fsm_{std::make_shared<fsm::DispatchFSM>()}
{
    start_FSM(master,
              options);
}

auto Dispatch::run() -> void
{
    // TODO: Is this appropriate here? Wouldn't it be better to leave the decision to act to the FSM?
    if(!has_nodes())
        return;

    dispatch_fsm_->process_event(fsm::poll{});
}

auto Dispatch::has_nodes() -> bool
{
    return !dispatch_fsm_->node_registrar().acquire()->nodes().empty();
}

auto Dispatch::start_FSM(Port master,
                         const option::Dispatch& options) -> void
{
    dispatch_fsm_->start();

    dispatch_fsm_->process_event(fsm::start{master,
                                            options});
}

auto filter_vm(const NodeRegistrar::Nodes& nodes) -> NodeRegistrar::Nodes
{
    auto res = decltype(nodes){};

    std::copy_if(nodes.begin(),
                 nodes.end(),
                 std::back_inserter(res),
                 [](const NodeRegistrar::Node& node) {
        return node->acquire()->type != packet_type::cluster_request_vm_node;
    });

    return res;
}

auto filter_svm(const NodeRegistrar::Nodes& nodes) -> NodeRegistrar::Nodes
{
    auto res = decltype(nodes){};

    std::copy_if(nodes.begin(),
                 nodes.end(),
                 std::back_inserter(res),
                 [](const NodeRegistrar::Node& node) {
        return node->acquire()->type != packet_type::cluster_request_svm_node;
    });

    return res;
}

auto sort_by_trace(NodeRegistrar::Nodes& nodes) -> void
{
    std::sort(nodes.begin(),
              nodes.end(),
              [](const NodeRegistrar::Node& lhs,
                 const NodeRegistrar::Node& rhs) {
        return lhs->acquire()->status.trace_count < rhs->acquire()->status.trace_count;
    });
}

auto sort_by_test(NodeRegistrar::Nodes& nodes) -> void
{
    std::sort(nodes.begin(),
              nodes.end(),
              [](const NodeRegistrar::Node& lhs,
                 const NodeRegistrar::Node& rhs) {
        return lhs->acquire()->status.test_case_count < rhs->acquire()->status.test_case_count;
    });
}

auto receive_traces(NodeRegistrar::Node& node) -> std::vector<Trace>
{
    auto lock = node->acquire();

    auto pkinfo = PacketInfo{0,0,0};
    pkinfo.id = lock->status.id;
    pkinfo.type = packet_type::cluster_trace_request;


    lock->server.write(pkinfo);

    auto traces = std::vector<Trace>{};

    read_serialized_binary(lock->server,
                           traces,
                           packet_type::cluster_trace);

    return traces;
}

auto receive_tests(NodeRegistrar::Node& node) -> std::vector<TestCase>
{
    auto lock = node->acquire();

    auto pkinfo = PacketInfo{0,0,0};
    pkinfo.id = lock->status.id;
    pkinfo.type = packet_type::cluster_test_case_request;

    lock->server.write(pkinfo);

    auto tcs = std::vector<TestCase>{};

    read_serialized_binary(lock->server,
                           tcs,
                           packet_type::cluster_test_case);

    return tcs;
}

auto receive_errors(NodeRegistrar::Node& node) -> std::vector<log::NodeError>
{
    auto lock = node->acquire();

    auto pkinfo = PacketInfo{0,0,0};
    pkinfo.id = lock->status.id;
    pkinfo.type = packet_type::cluster_error_log_request;

    lock->server.write(pkinfo);

    auto errs = std::vector<log::NodeError>{};

    read_serialized_binary(lock->server,
                           errs,
                           packet_type::cluster_test_case);

    return errs;
}

auto receive_image_info(NodeRegistrar::Node& node) -> ImageInfo
{
    auto pkinfo = PacketInfo{0,0,0};
    auto image_info = ImageInfo{};
    auto lock = node->acquire();

    pkinfo.id = lock->status.id;
    pkinfo.size = 0;
    pkinfo.type = packet_type::cluster_image_info_request;

    lock->server.write(pkinfo);

    read_serialized_binary(lock->server,
                           image_info,
                           packet_type::cluster_image_info);

    return image_info;
}

auto transmit_traces(NodeRegistrar::Node& node,
                     const std::vector<Trace>& traces) -> void
{
    if(traces.empty())
    {
        return;
    }

    auto lock = node->acquire();

    auto pkinfo = PacketInfo{0,0,0};
    pkinfo.id = lock->status.id;
    pkinfo.type = packet_type::cluster_trace;

    write_serialized_binary(lock->server,
                            pkinfo,
                            traces);
}

auto transmit_tests(NodeRegistrar::Node& node,
                   const std::vector<TestCase>& tcs) -> void
{
    if(tcs.empty())
    {
        return;
    }

    auto lock = node->acquire();

    auto pkinfo = PacketInfo{0,0,0};
    pkinfo.id = lock->status.id;
    pkinfo.type = packet_type::cluster_test_case;

    write_serialized_binary(lock->server,
                            pkinfo,
                            tcs);
}

auto transmit_commencement(NodeRegistrar::Node& node) -> void
{
    auto lock = node->acquire();
    lock->server.write(lock->status.id,
                       packet_type::cluster_commence);
}

auto transmit_image_info(NodeRegistrar::Node& node,
                         const ImageInfo& ii) -> void
{
    auto lock = node->acquire();

    auto pkinfo = PacketInfo{0,0,0};
    pkinfo.id = lock->status.id;
    pkinfo.type = packet_type::cluster_image_info;

    write_serialized_binary(lock->server,
                            pkinfo,
                            ii);
}

auto transmit_config(NodeRegistrar::Node& node,
                     const option::Dispatch& options) -> void
{
    auto lock = node->acquire();

    auto pkinfo = PacketInfo{0,0,0};
    pkinfo.id = lock->status.id;
    pkinfo.type = packet_type::cluster_config;

    write_serialized_binary(lock->server,
                            pkinfo,
                            options);

}

auto register_node_fsm(NodeRegistrar::Node& node,
                       const option::Dispatch& options,
                       AtomicGuard<fsm::DispatchFSM::VMNodeFSMs>& vm_node_fsms,
                       AtomicGuard<fsm::DispatchFSM::SVMNodeFSMs>& svm_node_fsms) -> void
{
    node->acquire()->status.active = true;

    switch(node->acquire()->type)
    {
    case packet_type::cluster_request_vm_node:
    {
        auto fsm = std::make_shared<vm::NodeFSM>();
        fsm->start();
        fsm->process_event(vm::start{node,
                                     false, // TODO: depends on a few criteria. For now, make no node 'first.'
                                     options.vm.image.update,
                                     options.mode.distributed});
        vm_node_fsms.acquire()->emplace_back(fsm);
        break;
    }
    case packet_type::cluster_request_svm_node:
    {
        auto fsm = std::make_shared<svm::NodeFSM>();
        fsm->start();
        fsm->process_event(svm::start{node});
        svm_node_fsms.acquire()->emplace_back(fsm);
        break;
    }
    default:
    {
        BOOST_THROW_EXCEPTION(Exception{} << err::msg{"node type not recognized"});
        break;
    }
    }
}

auto make_dispatch_root() -> boost::filesystem::path
{
    using namespace boost::posix_time;
    using namespace boost::gregorian;

    auto p = fs::path{dispatch_root_dir_name};

    auto now = second_clock::local_time();
    auto s = to_simple_string(now);

    std::replace(s.begin(), s.end(), ' ', '_');
    std::replace(s.begin(), s.end(), ':', '.');

    return p / s;
}

} // namespace cluster
} // namespace crete
