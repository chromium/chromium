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
#include "base/memory/scoped_refptr.h"
#include "base/process/process.h"
#include "base/strings/strcat.h"
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

const char kMojoIpczInProcessTestDriverName[] = "MojoIpczInProcess";
const char kMojoIpczMultiprocessTestDriverName[] = "MojoIpczMultiprocess";

class MojoIpczInProcessTestNodeController
    : public ipcz::test::TestNode::TestNodeController {
 public:
  class NodeThreadDelegate : public base::DelegateSimpleThread::Delegate {
   public:
    NodeThreadDelegate(std::unique_ptr<ipcz::test::TestNode> node,
                       ipcz::test::TestDriver* driver)
        : node_(std::move(node)), driver_(driver) {}

    // base::DelegateSimpleThread::Delegate:
    void Run() override {
      node_->Initialize(driver_);
      node_->NodeBody();
      node_.reset();
    }

   private:
    std::unique_ptr<ipcz::test::TestNode> node_;
    ipcz::test::TestDriver* const driver_;
  };

  MojoIpczInProcessTestNodeController(
      const std::string& node_name,
      std::unique_ptr<ipcz::test::TestNode> test_node,
      ipcz::test::TestDriver* test_driver)
      : node_thread_delegate_(std::move(test_node), test_driver),
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

 private:
  ~MojoIpczInProcessTestNodeController() override {
    CHECK(node_thread_.HasBeenJoined());
  }

  NodeThreadDelegate node_thread_delegate_;
  base::DelegateSimpleThread node_thread_;
};

class MojoIpczChildTestNodeController
    : public ipcz::test::TestNode::TestNodeController {
 public:
  explicit MojoIpczChildTestNodeController(base::Process process)
      : process_(std::move(process)) {}

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

 private:
  ~MojoIpczChildTestNodeController() override { DCHECK(result_.has_value()); }

  base::Process process_;
  absl::optional<bool> result_;
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

  ipcz::test::TestNode::TransportPair CreateTransports(
      ipcz::test::TestNode& source) const override {
    auto [ours, theirs] =
        Transport::CreatePair(Transport::kToNonBroker, Transport::kToBroker);
    return {
        .ours = Transport::ReleaseAsHandle(std::move(ours)),
        .theirs = Transport::ReleaseAsHandle(std::move(theirs)),
    };
  }

  ipcz::Ref<ipcz::test::TestNode::TestNodeController> SpawnTestNode(
      ipcz::test::TestNode& source,
      const ipcz::test::TestNodeDetails& details,
      IpczDriverHandle our_transport,
      IpczDriverHandle their_transport) override {
    if (mode_ == kInProcess) {
      return SpawnTestNodeThread(source, details, our_transport,
                                 their_transport);
    }
    return SpawnTestNodeProcess(source, details, our_transport,
                                their_transport);
  }

  IpczConnectNodeFlags GetExtraClientConnectNodeFlags() const override {
    return IPCZ_NO_FLAGS;
  }

  IpczDriverHandle GetClientTestNodeTransport() override {
    PlatformChannelEndpoint endpoint =
        PlatformChannelEndpoint::RecoverFromString(
            base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                PlatformChannel::kHandleSwitch));
    return Transport::ReleaseAsHandle(base::MakeRefCounted<Transport>(
        Transport::kToBroker, std::move(endpoint)));
  }

 private:
  ipcz::Ref<ipcz::test::TestNode::TestNodeController> SpawnTestNodeThread(
      ipcz::test::TestNode& source,
      const ipcz::test::TestNodeDetails& details,
      IpczDriverHandle our_transport,
      IpczDriverHandle their_transport) {
    std::unique_ptr<ipcz::test::TestNode> node = details.factory();
    node->SetTransport(their_transport);
    Transport::FromHandle(our_transport)
        ->set_remote_process(base::Process::Current());
    return ipcz::MakeRefCounted<MojoIpczInProcessTestNodeController>(
        std::string(details.name.begin(), details.name.end()), std::move(node),
        this);
  }

  ipcz::Ref<ipcz::test::TestNode::TestNodeController> SpawnTestNodeProcess(
      ipcz::test::TestNode& source,
      const ipcz::test::TestNodeDetails& details,
      IpczDriverHandle our_transport,
      IpczDriverHandle their_transport) {
    const std::string test_child_main = base::StrCat(
        {details.name.data(), "/", kMojoIpczMultiprocessTestDriverName});
    base::CommandLine command_line(
        base::GetMultiProcessTestChildBaseCommandLine().GetProgram());

    std::set<std::string> uninherited_args;
    uninherited_args.insert(PlatformChannel::kHandleSwitch);
    uninherited_args.insert(switches::kTestChildProcess);

    // Copy commandline switches from the parent process, except for the
    // multiprocess client name and mojo message pipe handle; this allows test
    // clients to spawn other test clients.
    for (const auto& entry :
         base::CommandLine::ForCurrentProcess()->GetSwitches()) {
      if (uninherited_args.find(entry.first) == uninherited_args.end())
        command_line.AppendSwitchNative(entry.first, entry.second);
    }

    base::LaunchOptions options;
    scoped_refptr<Transport> transport =
        Transport::TakeFromHandle(their_transport);
    PlatformChannelEndpoint endpoint = transport->TakeEndpoint();
    endpoint.PrepareToPass(options, command_line);
#if BUILDFLAG(IS_WIN)
    options.start_hidden = true;
#endif

    base::Process child = base::SpawnMultiProcessTestChild(
        test_child_main, command_line, options);
    endpoint.ProcessLaunchAttempted();
    Transport::FromHandle(our_transport)->set_remote_process(child.Duplicate());
    return ipcz::MakeRefCounted<MojoIpczChildTestNodeController>(
        std::move(child));
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
