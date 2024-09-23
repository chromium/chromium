// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/base_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/scoped_refptr.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "base/threading/simple_thread.h"
#include "build/build_config.h"
#include "mojo/core/ipcz_driver/driver.h"
#include "mojo/core/ipcz_driver/transport.h"
#include "mojo/public/cpp/platform/platform_channel.h"
#include "mojo/public/cpp/platform/platform_channel_endpoint.h"
#include "third_party/ipcz/src/test/multinode_test.h"

namespace mojo::core::ipcz_driver {
namespace {

const char kParentHandle[] = "mojo-ipcz-test-parent-handle";

const char kMojoIpczInProcessTestDriverName[] = "MojoIpczInProcess";
const char kMojoIpczMultiprocessTestDriverName[] = "MojoIpczMultiprocess";

class MojoIpczInProcessTestNodeController
    : public ipcz::test::TestNode::TestNodeController {
 public:
  class NodeThreadDelegate : public base::DelegateSimpleThread::Delegate {
   public:
    NodeThreadDelegate(std::unique_ptr<ipcz::test::TestNode> node,
                       ipcz::test::TestDriver* driver,
                       const std::string& feature_set)
        : node_(std::move(node)), driver_(driver), feature_set_(feature_set) {}

    // base::DelegateSimpleThread::Delegate:
    void Run() override {
      node_->Initialize(driver_, feature_set_);
      node_->NodeBody();
      node_.reset();
    }

   private:
    std::unique_ptr<ipcz::test::TestNode> node_;
    const raw_ptr<ipcz::test::TestDriver> driver_;
    const std::string feature_set_;
  };

  MojoIpczInProcessTestNodeController(
      ipcz::test::TestNode& source,
      const std::string& node_name,
      std::unique_ptr<ipcz::test::TestNode> test_node,
      ipcz::test::TestDriver* test_driver,
      const std::string& feature_set)
      : source_(source),
        is_broker_(test_node->GetDetails().is_broker),
        node_thread_delegate_(std::move(test_node), test_driver, feature_set),
        node_thread_(&node_thread_delegate_, node_name) {
    node_thread_.StartAsync();
  }

  // TestNode::TestNodeController:
  bool WaitForShutdown() override {
    if (!node_thread_.HasBeenJoined()) {
      node_thread_.Join();
    }
    return true;
  }

  ipcz::test::TransportPair CreateNewTransports() override {
    ipcz::test::TransportPair transports;
    if (is_broker_) {
      transports = source_->CreateBrokerToBrokerTransports();
    } else {
      transports = source_->CreateTransports();
    }

    Transport::FromHandle(transports.ours)
        ->set_remote_process(base::Process::Current());
    Transport::FromHandle(transports.theirs)
        ->set_remote_process(base::Process::Current());
    return transports;
  }

 private:
  ~MojoIpczInProcessTestNodeController() override {
    CHECK(node_thread_.HasBeenJoined());
  }

  const raw_ref<ipcz::test::TestNode> source_;
  const bool is_broker_;
  NodeThreadDelegate node_thread_delegate_;
  base::DelegateSimpleThread node_thread_;
};

class MojoIpczChildTestNodeController
    : public ipcz::test::TestNode::TestNodeController {
 public:
  MojoIpczChildTestNodeController(
      ipcz::test::TestNode& source,
      const ipcz::test::TestNodeDetails& child_details,
      base::Process process)
      : source_(source),
        is_broker_(child_details.is_broker),
        process_(std::move(process)) {}

  // ipcz::test::TestNode::TestNodeController:
  bool WaitForShutdown() override {
    if (!process_.IsValid()) {
      DCHECK(result_);
      return *result_;
    }

    int rv = -1;
    base::WaitForMultiprocessTestChildExit(process_,
                                           TestTimeouts::action_timeout(), &rv);
    process_.Close();
    result_ = (rv == 0);
    return *result_;
  }

  ipcz::test::TransportPair CreateNewTransports() override {
    ipcz::test::TransportPair transports;
    if (is_broker_) {
      transports = source_->CreateBrokerToBrokerTransports();
    } else {
      transports = source_->CreateTransports();
    }

    Transport::FromHandle(transports.ours)
        ->set_remote_process(process_.Duplicate());
    return transports;
  }

 private:
  ~MojoIpczChildTestNodeController() override { DCHECK(result_.has_value()); }

  const raw_ref<ipcz::test::TestNode> source_;
  const bool is_broker_;
  base::Process process_;
  std::optional<bool> result_;
};

// TestDriver implementation for the mojo-ipcz driver to have coverage in ipcz'
// multinode tests.
class MojoIpczTestDriver : public ipcz::test::TestDriver {
 public:
  enum Mode {
    kInProcess,
    kMultiprocess,
  };
  explicit MojoIpczTestDriver(Mode mode) : mode_(mode) {}

  const IpczDriver& GetIpczDriver() const override { return kDriver; }

  const char* GetName() const override {
    if (mode_ == kInProcess) {
      return kMojoIpczInProcessTestDriverName;
    }
    return kMojoIpczMultiprocessTestDriverName;
  }

  ipcz::test::TransportPair CreateTransports(
      ipcz::test::TestNode& source,
      bool for_broker_target) const override {
    std::pair<scoped_refptr<Transport>, scoped_refptr<Transport>> transports;
    transports = Transport::CreatePair(
        Transport::kBroker,
        for_broker_target ? Transport::kBroker : Transport::kNonBroker);
    return {
        .ours = Transport::ReleaseAsHandle(std::move(transports.first)),
        .theirs = Transport::ReleaseAsHandle(std::move(transports.second)),
    };
  }

  ipcz::Ref<ipcz::test::TestNode::TestNodeController> SpawnTestNode(
      ipcz::test::TestNode& source,
      const ipcz::test::TestNodeDetails& details,
      const std::string& feature_set,
      IpczDriverHandle our_transport,
      IpczDriverHandle their_transport) override {
    if (mode_ == kInProcess) {
      return SpawnTestNodeThread(source, details, feature_set, our_transport,
                                 their_transport);
    }
    return SpawnTestNodeProcess(source, details, feature_set, our_transport,
                                their_transport);
  }

  IpczConnectNodeFlags GetExtraClientConnectNodeFlags() const override {
    return IPCZ_NO_FLAGS;
  }

  IpczDriverHandle GetClientTestNodeTransport() override {
    const base::CommandLine& command_line =
        *base::CommandLine::ForCurrentProcess();
    PlatformChannelEndpoint endpoint =
        PlatformChannelEndpoint::RecoverFromString(
            command_line.GetSwitchValueASCII(PlatformChannel::kHandleSwitch));

    base::Process parent_process;
#if BUILDFLAG(IS_WIN)
    // If we're launched as a broker, the test will pass us a handle back to its
    // process. The Transport uses this to duplicate handles to and from the
    // parent process. See SpawnTestNodeProcess().
    const std::string parent_handle_switch =
        command_line.GetSwitchValueASCII(kParentHandle);
    int parent_handle_value;
    if (!parent_handle_switch.empty() &&
        base::StringToInt(parent_handle_switch, &parent_handle_value)) {
      parent_process = base::Process(LongToHandle(parent_handle_value));
    }
#endif  // BUILDFLAG(IS_WIN)
    const bool is_broker = parent_process.IsValid();
    return Transport::ReleaseAsHandle(Transport::Create(
        {.source = is_broker ? Transport::kBroker : Transport::kNonBroker,
         .destination = Transport::kBroker},
        std::move(endpoint), std::move(parent_process)));
  }

 private:
  ipcz::Ref<ipcz::test::TestNode::TestNodeController> SpawnTestNodeThread(
      ipcz::test::TestNode& source,
      const ipcz::test::TestNodeDetails& details,
      const std::string& feature_set,
      IpczDriverHandle our_transport,
      IpczDriverHandle their_transport) {
    Transport::FromHandle(our_transport)
        ->set_remote_process(base::Process::Current());
    if (details.is_broker) {
      Transport::FromHandle(their_transport)
          ->set_remote_process(base::Process::Current());
    }
    std::unique_ptr<ipcz::test::TestNode> node = details.factory();
    node->SetTransport(their_transport);
    return ipcz::MakeRefCounted<MojoIpczInProcessTestNodeController>(
        source, std::string(details.name.begin(), details.name.end()),
        std::move(node), this, feature_set);
  }

  ipcz::Ref<ipcz::test::TestNode::TestNodeController> SpawnTestNodeProcess(
      ipcz::test::TestNode& source,
      const ipcz::test::TestNodeDetails& details,
      const std::string& feature_set,
      IpczDriverHandle our_transport,
      IpczDriverHandle their_transport) {
    const std::string test_child_main =
        base::StrCat({details.name.data(), "/",
                      kMojoIpczMultiprocessTestDriverName, "_", feature_set});
    base::CommandLine command_line(
        base::GetMultiProcessTestChildBaseCommandLine().GetProgram());

    std::set<std::string> uninherited_args;
    uninherited_args.insert(PlatformChannel::kHandleSwitch);
    uninherited_args.insert(kParentHandle);
    uninherited_args.insert(switches::kTestChildProcess);

    // Copy commandline switches from the parent process, except for the
    // multiprocess client name and mojo message pipe handle; this allows test
    // clients to spawn other test clients.
    for (const auto& entry :
         base::CommandLine::ForCurrentProcess()->GetSwitches()) {
      if (!base::Contains(uninherited_args, entry.first)) {
        command_line.AppendSwitchNative(entry.first, entry.second);
      }
    }

    base::LaunchOptions options;
    scoped_refptr<Transport> transport =
        Transport::TakeFromHandle(their_transport);
    PlatformChannelEndpoint endpoint = transport->TakeEndpoint();
    endpoint.PrepareToPass(options, command_line);
#if BUILDFLAG(IS_WIN)
    options.start_hidden = true;

    base::Process this_process;
    if (details.is_broker) {
      // If we're launching another broker, it needs a handle back to our own
      // process so that it can duplicate handles between us and itself. See
      // GetClientTestNodeTransport().
      HANDLE dupe;
      BOOL ok = ::DuplicateHandle(::GetCurrentProcess(), ::GetCurrentProcess(),
                                  ::GetCurrentProcess(), &dupe, 0, TRUE,
                                  DUPLICATE_SAME_ACCESS);
      CHECK(ok);
      this_process = base::Process(dupe);

      options.handles_to_inherit.push_back(this_process.Handle());
      command_line.AppendSwitchASCII(
          kParentHandle,
          base::NumberToString(HandleToLong(this_process.Handle())));
    }
#endif

    base::Process child = base::SpawnMultiProcessTestChild(
        test_child_main, command_line, options);
    endpoint.ProcessLaunchAttempted();
    Transport::FromHandle(our_transport)->set_remote_process(child.Duplicate());
    return ipcz::MakeRefCounted<MojoIpczChildTestNodeController>(
        source, details, std::move(child));
  }

  const Mode mode_;
};

ipcz::test::TestDriverRegistration<MojoIpczTestDriver> kRegisterInProcessDriver{
    MojoIpczTestDriver::kInProcess};

#if !BUILDFLAG(IS_IOS)
ipcz::test::TestDriverRegistration<MojoIpczTestDriver>
    kRegisterMultiprocessDriver{MojoIpczTestDriver::kMultiprocess};
#endif

}  // namespace
}  // namespace mojo::core::ipcz_driver
