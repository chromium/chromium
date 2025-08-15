// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include "ipc/ipc_channel_mojo.h"

#include <stddef.h>
#include <stdint.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>

#include "base/base_paths.h"
#include "base/containers/queue.h"
#include "base/containers/span.h"
#include "base/files/file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/unsafe_shared_memory_region.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/message_loop/message_pump_type.h"
#include "base/path_service.h"
#include "base/pickle.h"
#include "base/process/process.h"
#include "base/run_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_io_thread.h"
#include "base/test/test_shared_memory_util.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "ipc/ipc_channel_mojo_unittest.test-mojom.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_message_utils.h"
#include "ipc/ipc_mojo_handle_attachment.h"
#include "ipc/ipc_mojo_message_helper.h"
#include "ipc/ipc_mojo_param_traits.h"
#include "ipc/ipc_sync_channel.h"
#include "ipc/ipc_sync_message.h"
#include "ipc/ipc_test.test-mojom.h"
#include "ipc/ipc_test_base.h"
#include "ipc/urgent_message_observer.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/features.h"
#include "mojo/public/cpp/bindings/lib/validation_errors.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/bindings/self_owned_associated_receiver.h"
#include "mojo/public/cpp/bindings/urgent_message_scope.h"
#include "mojo/public/cpp/system/functions.h"
#include "mojo/public/cpp/system/wait.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include "base/file_descriptor_posix.h"
#include "ipc/ipc_platform_file_attachment_posix.h"
#endif

namespace ipc_channel_mojo_unittest {
namespace {

class TestListenerBase : public IPC::Listener {
 public:
  explicit TestListenerBase(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}

  ~TestListenerBase() override = default;
  void OnChannelError() override { RunQuitClosure(); }

  void set_sender(IPC::Sender* sender) { sender_ = sender; }
  IPC::Sender* sender() const { return sender_; }
  void RunQuitClosure() {
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

 private:
  raw_ptr<IPC::Sender> sender_ = nullptr;
  base::OnceClosure quit_closure_;
};

using IPCChannelMojoTest = IPCChannelMojoTestBase;

class ListenerThatQuits : public IPC::Listener {
 public:
  explicit ListenerThatQuits(base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}

  bool OnMessageReceived(const IPC::Message& message) override { return true; }

  void OnChannelConnected(int32_t peer_pid) override {
    std::move(quit_closure_).Run();
  }

 private:
  base::OnceClosure quit_closure_;
};

class ChannelProxyRunner {
 public:
  ChannelProxyRunner(mojo::ScopedMessagePipeHandle handle,
                     bool for_server)
      : for_server_(for_server),
        handle_(std::move(handle)),
        io_thread_("ChannelProxyRunner IO thread"),
        never_signaled_(base::WaitableEvent::ResetPolicy::MANUAL,
                        base::WaitableEvent::InitialState::NOT_SIGNALED) {
  }

  ChannelProxyRunner(const ChannelProxyRunner&) = delete;
  ChannelProxyRunner& operator=(const ChannelProxyRunner&) = delete;

  void CreateProxy(
      IPC::Listener* listener,
      IPC::UrgentMessageObserver* urgent_message_observer = nullptr) {
    io_thread_.StartWithOptions(
        base::Thread::Options(base::MessagePumpType::IO, 0));
    proxy_ = IPC::SyncChannel::Create(
        listener, io_thread_.task_runner(),
        base::SingleThreadTaskRunner::GetCurrentDefault(), &never_signaled_);
    proxy_->SetUrgentMessageObserver(urgent_message_observer);
  }

  void RunProxy() {
    std::unique_ptr<IPC::ChannelFactory> factory;
    if (for_server_) {
      factory = IPC::ChannelMojo::CreateServerFactory(
          std::move(handle_), io_thread_.task_runner(),
          base::SingleThreadTaskRunner::GetCurrentDefault());
    } else {
      factory = IPC::ChannelMojo::CreateClientFactory(
          std::move(handle_), io_thread_.task_runner(),
          base::SingleThreadTaskRunner::GetCurrentDefault());
    }
    proxy_->Init(std::move(factory), true);
  }

  IPC::ChannelProxy* proxy() { return proxy_.get(); }

 private:
  const bool for_server_;

  mojo::ScopedMessagePipeHandle handle_;
  base::Thread io_thread_;
  base::WaitableEvent never_signaled_;
  std::unique_ptr<IPC::ChannelProxy> proxy_;
};

class IPCChannelProxyMojoTest : public IPCChannelMojoTestBase {
 public:
  void Init(const std::string& client_name) {
    IPCChannelMojoTestBase::Init(client_name);
    runner_ = std::make_unique<ChannelProxyRunner>(TakeHandle(), true);
  }

  void CreateProxy(
      IPC::Listener* listener,
      IPC::UrgentMessageObserver* urgent_message_observer = nullptr) {
    runner_->CreateProxy(listener, urgent_message_observer);
  }

  void RunProxy() {
    runner_->RunProxy();
  }

  void DestroyProxy() {
    runner_.reset();
    base::RunLoop().RunUntilIdle();
  }

  IPC::ChannelProxy* proxy() { return runner_->proxy(); }

 private:
  std::unique_ptr<ChannelProxyRunner> runner_;
};

class ChannelProxyClient {
 public:
  void Init(mojo::ScopedMessagePipeHandle handle) {
    runner_ = std::make_unique<ChannelProxyRunner>(std::move(handle), false);
  }

  void CreateProxy(IPC::Listener* listener) { runner_->CreateProxy(listener); }

  void RunProxy() { runner_->RunProxy(); }

  void DestroyProxy() {
    runner_.reset();
    base::RunLoop().RunUntilIdle();
  }

  void RequestQuitAndWaitForAck(IPC::mojom::SimpleTestDriver* driver) {
    base::RunLoop loop;
    driver->RequestQuit(loop.QuitClosure());
    loop.Run();
  }

  IPC::ChannelProxy* proxy() { return runner_->proxy(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<ChannelProxyRunner> runner_;
};

class DummyListener : public IPC::Listener {
 public:
  // IPC::Listener
  bool OnMessageReceived(const IPC::Message& message) override { return true; }
};

class ListenerWithIndirectProxyAssociatedInterface
    : public IPC::Listener,
      public IPC::mojom::IndirectTestDriver,
      public IPC::mojom::PingReceiver {
 public:
  ListenerWithIndirectProxyAssociatedInterface() = default;
  ~ListenerWithIndirectProxyAssociatedInterface() override = default;

  // IPC::Listener:
  bool OnMessageReceived(const IPC::Message& message) override { return true; }

  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override {
    DCHECK(!driver_receiver_.is_bound());
    DCHECK_EQ(interface_name, IPC::mojom::IndirectTestDriver::Name_);
    driver_receiver_.Bind(
        mojo::PendingAssociatedReceiver<IPC::mojom::IndirectTestDriver>(
            std::move(handle)));
  }

  void set_ping_handler(const base::RepeatingClosure& handler) {
    ping_handler_ = handler;
  }

 private:
  // IPC::mojom::IndirectTestDriver:
  void GetPingReceiver(mojo::PendingAssociatedReceiver<IPC::mojom::PingReceiver>
                           receiver) override {
    ping_receiver_receiver_.Bind(std::move(receiver));
  }

  // IPC::mojom::PingReceiver:
  void Ping(PingCallback callback) override {
    std::move(callback).Run();
    ping_handler_.Run();
  }

  mojo::AssociatedReceiver<IPC::mojom::IndirectTestDriver> driver_receiver_{
      this};
  mojo::AssociatedReceiver<IPC::mojom::PingReceiver> ping_receiver_receiver_{
      this};

  base::RepeatingClosure ping_handler_;
};

TEST_F(IPCChannelProxyMojoTest, ProxyThreadAssociatedInterfaceIndirect) {
  // Tests that we can pipeline interface requests and subsequent messages
  // targeting proxy thread bindings, and the channel will still dispatch
  // messages appropriately.

  Init("ProxyThreadAssociatedInterfaceIndirectClient");

  ListenerWithIndirectProxyAssociatedInterface listener;
  CreateProxy(&listener);
  RunProxy();

  base::RunLoop loop;
  listener.set_ping_handler(loop.QuitClosure());
  loop.Run();

  EXPECT_TRUE(WaitForClientShutdown());

  DestroyProxy();
}

DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT_WITH_CUSTOM_FIXTURE(
    ProxyThreadAssociatedInterfaceIndirectClient,
    ChannelProxyClient) {
  DummyListener listener;
  CreateProxy(&listener);
  RunProxy();

  // Use an interface requested via another interface. On the remote end both
  // interfaces are bound on the proxy thread. This ensures that the Ping
  // message we send will still be dispatched properly even though the remote
  // endpoint may not have been bound yet by the time the message is initially
  // processed on the IO thread.
  mojo::AssociatedRemote<IPC::mojom::IndirectTestDriver> driver;
  mojo::AssociatedRemote<IPC::mojom::PingReceiver> ping_receiver;
  proxy()->GetRemoteAssociatedInterface(
      driver.BindNewEndpointAndPassReceiver());
  driver->GetPingReceiver(ping_receiver.BindNewEndpointAndPassReceiver());

  base::RunLoop loop;
  ping_receiver->Ping(loop.QuitClosure());
  loop.Run();

  DestroyProxy();
}

TEST_F(IPCChannelProxyMojoTest, AssociatedRequestClose) {
  Init("DropAssociatedRequest");

  DummyListener listener;
  CreateProxy(&listener);
  RunProxy();

  mojo::AssociatedRemote<IPC::mojom::AssociatedInterfaceVendor> vendor;
  proxy()->GetRemoteAssociatedInterface(
      vendor.BindNewEndpointAndPassReceiver());
  mojo::AssociatedRemote<IPC::mojom::SimpleTestDriver> tester;
  vendor->GetTestInterface(tester.BindNewEndpointAndPassReceiver());
  base::RunLoop run_loop;
  tester.set_disconnect_handler(run_loop.QuitClosure());
  run_loop.Run();

  tester.reset();
  proxy()->GetRemoteAssociatedInterface(
      tester.BindNewEndpointAndPassReceiver());
  EXPECT_TRUE(WaitForClientShutdown());
  DestroyProxy();
}

class AssociatedInterfaceDroppingListener : public IPC::Listener {
 public:
  AssociatedInterfaceDroppingListener(base::OnceClosure callback)
      : callback_(std::move(callback)) {}
  bool OnMessageReceived(const IPC::Message& message) override { return false; }

  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override {
    if (interface_name == IPC::mojom::SimpleTestDriver::Name_)
      std::move(callback_).Run();
  }

 private:
  base::OnceClosure callback_;
};

DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT_WITH_CUSTOM_FIXTURE(DropAssociatedRequest,
                                                        ChannelProxyClient) {
  base::RunLoop run_loop;
  AssociatedInterfaceDroppingListener listener(run_loop.QuitClosure());
  CreateProxy(&listener);
  RunProxy();
  run_loop.Run();
  DestroyProxy();
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

const base::ProcessId kMagicChildId = 54321;

class ListenerThatVerifiesPeerPid : public TestListenerBase {
 public:
  explicit ListenerThatVerifiesPeerPid(base::OnceClosure quit_closure)
      : TestListenerBase(std::move(quit_closure)) {}

  void OnChannelConnected(int32_t peer_pid) override {
    EXPECT_EQ(peer_pid, kMagicChildId);
    RunQuitClosure();
  }

  bool OnMessageReceived(const IPC::Message& message) override { NOTREACHED(); }
};

// The global PID is only used on systems that use the zygote. Hence, this
// test is disabled on other platforms.
TEST_F(IPCChannelMojoTest, VerifyGlobalPid) {
  Init("IPCChannelMojoTestVerifyGlobalPidClient");

  base::RunLoop run_loop;
  ListenerThatVerifiesPeerPid listener(run_loop.QuitClosure());
  CreateChannel(&listener);
  ASSERT_TRUE(ConnectChannel());

  run_loop.Run();
  channel()->Close();

  EXPECT_TRUE(WaitForClientShutdown());
  DestroyChannel();
}

DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT(IPCChannelMojoTestVerifyGlobalPidClient) {
  IPC::Channel::SetGlobalPid(kMagicChildId);

  base::RunLoop run_loop;
  ListenerThatQuits listener(run_loop.QuitClosure());
  Connect(&listener);

  run_loop.Run();

  Close();
}

#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

class ListenerWithUrgentMessageAssociatedInterface
    : public IPC::mojom::InterfaceWithUrgentMethod,
      public IPC::Listener,
      public IPC::UrgentMessageObserver {
 public:
  explicit ListenerWithUrgentMessageAssociatedInterface(
      base::OnceClosure quit_closure)
      : quit_closure_(std::move(quit_closure)) {}

  ListenerWithUrgentMessageAssociatedInterface(
      const ListenerWithUrgentMessageAssociatedInterface&) = delete;
  ListenerWithUrgentMessageAssociatedInterface& operator=(
      const ListenerWithUrgentMessageAssociatedInterface&) = delete;

  ~ListenerWithUrgentMessageAssociatedInterface() override = default;

  uint32_t num_maybe_urgent_messages_received() const {
    return num_maybe_urgent_messages_received_;
  }

  uint32_t num_urgent_messages_received() const {
    return num_urgent_messages_received_;
  }

  uint32_t num_non_urgent_messages_received() const {
    return num_non_urgent_messages_received_;
  }

  uint32_t num_observer_urgent_messages_received() const {
    return num_observer_urgent_messages_received_.load(
        std::memory_order_relaxed);
  }

  uint32_t num_observer_urgent_messages_processed() const {
    return num_observer_urgent_messages_processed_.load(
        std::memory_order_relaxed);
  }

  bool was_process_callback_pending_during_ipc_dispatch() const {
    return was_process_callback_pending_during_ipc_dispatch_;
  }

 private:
  // IPC::mojom::InterfaceWithUrgentMethod:
  void MaybeUrgentMessage(bool is_urgent) override {
    ++num_maybe_urgent_messages_received_;
    if (!is_urgent) {
      return;
    }
    ++num_urgent_messages_received_;
    uint32_t received =
        num_observer_urgent_messages_received_.load(std::memory_order_relaxed);
    uint32_t processed =
        num_observer_urgent_messages_processed_.load(std::memory_order_relaxed);
    // The "processed" observer callback should run after the IPC is dispatched,
    // so there should always be at least one less processed callback here.
    was_process_callback_pending_during_ipc_dispatch_ =
        was_process_callback_pending_during_ipc_dispatch_ &&
        (processed < received);
  }

  void NonUrgentMessage() override { ++num_non_urgent_messages_received_; }

  void RequestQuit(RequestQuitCallback callback) override {
    std::move(quit_closure_).Run();
    std::move(callback).Run();
  }

  // IPC::Listener:
  bool OnMessageReceived(const IPC::Message& message) override { return true; }

  void OnAssociatedInterfaceRequest(
      const std::string& interface_name,
      mojo::ScopedInterfaceEndpointHandle handle) override {
    CHECK(!receiver_.is_bound());
    CHECK_EQ(interface_name, IPC::mojom::InterfaceWithUrgentMethod::Name_);
    receiver_.Bind(
        mojo::PendingAssociatedReceiver<IPC::mojom::InterfaceWithUrgentMethod>(
            std::move(handle)));
  }

  // IPC::UrgentMessageObserver:
  void OnUrgentMessageReceived() override {
    std::atomic_fetch_add_explicit(&num_observer_urgent_messages_received_,
                                   uint32_t(1), std::memory_order_relaxed);
  }

  void OnUrgentMessageProcessed() override {
    std::atomic_fetch_add_explicit(&num_observer_urgent_messages_processed_,
                                   uint32_t(1), std::memory_order_relaxed);
  }

  base::OnceClosure quit_closure_;
  mojo::AssociatedReceiver<IPC::mojom::InterfaceWithUrgentMethod> receiver_{
      this};
  uint32_t num_maybe_urgent_messages_received_{0};
  uint32_t num_urgent_messages_received_{0};
  uint32_t num_non_urgent_messages_received_{0};
  std::atomic<uint32_t> num_observer_urgent_messages_received_{0};
  std::atomic<uint32_t> num_observer_urgent_messages_processed_{0};
  bool was_process_callback_pending_during_ipc_dispatch_{true};
};

TEST_F(IPCChannelProxyMojoTest, UrgentMessageObserver) {
  Init("UrgentMessageObserverClient");

  base::RunLoop run_loop;
  ListenerWithUrgentMessageAssociatedInterface listener(run_loop.QuitClosure());
  CreateProxy(&listener, /*urgent_message_observer=*/&listener);
  RunProxy();

  run_loop.Run();

  EXPECT_TRUE(WaitForClientShutdown());

  EXPECT_EQ(listener.num_maybe_urgent_messages_received(), 5u);
  EXPECT_EQ(listener.num_urgent_messages_received(), 3u);
  EXPECT_EQ(listener.num_non_urgent_messages_received(), 2u);

  EXPECT_EQ(listener.num_observer_urgent_messages_received(), 3u);
  EXPECT_EQ(listener.num_observer_urgent_messages_processed(), 3u);
  EXPECT_TRUE(listener.was_process_callback_pending_during_ipc_dispatch());

  DestroyProxy();
}

DEFINE_IPC_CHANNEL_MOJO_TEST_CLIENT_WITH_CUSTOM_FIXTURE(
    UrgentMessageObserverClient,
    ChannelProxyClient) {
  DummyListener listener;
  CreateProxy(&listener);
  RunProxy();

  mojo::AssociatedRemote<IPC::mojom::InterfaceWithUrgentMethod> remote;
  proxy()->GetRemoteAssociatedInterface(
      remote.BindNewEndpointAndPassReceiver());

  {
    mojo::UrgentMessageScope scope;
    remote->MaybeUrgentMessage(/*is_urgent=*/true);
  }
  remote->NonUrgentMessage();
  remote->MaybeUrgentMessage(/*is_urgent=*/false);
  {
    mojo::UrgentMessageScope scope;
    remote->MaybeUrgentMessage(/*is_urgent=*/true);
  }
  remote->NonUrgentMessage();
  remote->MaybeUrgentMessage(/*is_urgent=*/false);
  {
    mojo::UrgentMessageScope scope;
    remote->MaybeUrgentMessage(/*is_urgent=*/true);
  }

  base::RunLoop run_loop;
  remote->RequestQuit(run_loop.QuitClosure());
  run_loop.Run();

  DestroyProxy();
}

}  // namespace
}  // namespace ipc_channel_mojo_unittest
