// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ipc/ipc_mojo_bootstrap.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "ipc/ipc.mojom.h"
#include "ipc/ipc_test_base.h"
#include "mojo/core/test/multiprocess_test_helper.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"

namespace {

constexpr int32_t kTestServerPid = 42;
constexpr int32_t kTestClientPid = 4242;

class Connection {
 public:
  explicit Connection(std::unique_ptr<IPC::MojoBootstrap> bootstrap,
                      int32_t sender_id)
      : bootstrap_(std::move(bootstrap)) {
    mojo::PendingAssociatedRemote<IPC::mojom::Channel> sender;
    bootstrap_->Connect(&sender, &receiver_);
    sender_.Bind(std::move(sender));
    sender_->SetPeerPid(sender_id);

    // It's OK to start receiving right away even though `receiver_` isn't
    // bound, because all of these tests are single-threaded and it will be
    // bound before any incoming messages can be scheduled for processing.
    bootstrap_->StartReceiving();
  }

  void TakeReceiver(
      mojo::PendingAssociatedReceiver<IPC::mojom::Channel>* receiver) {
    *receiver = std::move(receiver_);
  }

  mojo::AssociatedRemote<IPC::mojom::Channel>& GetSender() { return sender_; }

 private:
  mojo::AssociatedRemote<IPC::mojom::Channel> sender_;
  mojo::PendingAssociatedReceiver<IPC::mojom::Channel> receiver_;
  std::unique_ptr<IPC::MojoBootstrap> bootstrap_;
};

class PeerPidReceiver : public IPC::mojom::Channel {
 public:
  enum class MessageExpectation {
    kNotExpected,
    kExpectedValid,
    kExpectedInvalid
  };

  PeerPidReceiver(
      mojo::PendingAssociatedReceiver<IPC::mojom::Channel> receiver,
      base::OnceClosure on_peer_pid_set,
      MessageExpectation message_expectation = MessageExpectation::kNotExpected)
      : receiver_(this, std::move(receiver)),
        on_peer_pid_set_(std::move(on_peer_pid_set)),
        message_expectation_(message_expectation) {
    receiver_.set_disconnect_handler(disconnect_run_loop_.QuitClosure());
  }

  PeerPidReceiver(const PeerPidReceiver&) = delete;
  PeerPidReceiver& operator=(const PeerPidReceiver&) = delete;

  ~PeerPidReceiver() override {
    bool expected_message =
        message_expectation_ != MessageExpectation::kNotExpected;
    EXPECT_EQ(expected_message, received_message_);
  }

  // mojom::Channel:
  void SetPeerPid(int32_t pid) override {
    peer_pid_ = pid;
    std::move(on_peer_pid_set_).Run();
  }

  void Receive(IPC::MessageView message_view) override {
    ASSERT_NE(MessageExpectation::kNotExpected, message_expectation_);
    received_message_ = true;

    IPC::Message message(
        reinterpret_cast<const char*>(message_view.bytes().data()),
        message_view.bytes().size());
    bool expected_valid =
        message_expectation_ == MessageExpectation::kExpectedValid;
    EXPECT_EQ(expected_valid, message.IsValid());
  }

  void GetAssociatedInterface(
      mojo::GenericPendingAssociatedReceiver receiver) override {}

  int32_t peer_pid() const { return peer_pid_; }

  void RunUntilDisconnect() { disconnect_run_loop_.Run(); }

 private:
  mojo::AssociatedReceiver<IPC::mojom::Channel> receiver_;
  base::OnceClosure on_peer_pid_set_;
  MessageExpectation message_expectation_;
  int32_t peer_pid_ = -1;
  bool received_message_ = false;
  base::RunLoop disconnect_run_loop_;
};

class IPCMojoBootstrapTest : public testing::Test {
 protected:
  mojo::core::test::MultiprocessTestHelper helper_;
};

TEST_F(IPCMojoBootstrapTest, Connect) {
  base::test::SingleThreadTaskEnvironment task_environment;
  Connection connection(IPC::MojoBootstrap::Create(
                            helper_.StartChild("IPCMojoBootstrapTestClient"),
                            IPC::Channel::MODE_SERVER,
                            base::SingleThreadTaskRunner::GetCurrentDefault(),
                            base::SingleThreadTaskRunner::GetCurrentDefault()),
                        kTestServerPid);

  mojo::PendingAssociatedReceiver<IPC::mojom::Channel> receiver;
  connection.TakeReceiver(&receiver);

  base::RunLoop run_loop;
  PeerPidReceiver impl(std::move(receiver), run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(kTestClientPid, impl.peer_pid());

  impl.RunUntilDisconnect();
  EXPECT_TRUE(helper_.WaitForChildTestShutdown());
}

// A long running process that connects to us.
MULTIPROCESS_TEST_MAIN_WITH_SETUP(
    IPCMojoBootstrapTestClientTestChildMain,
    ::mojo::core::test::MultiprocessTestHelper::ChildSetup) {
  base::test::SingleThreadTaskEnvironment task_environment;
  Connection connection(
      IPC::MojoBootstrap::Create(
          std::move(mojo::core::test::MultiprocessTestHelper::primordial_pipe),
          IPC::Channel::MODE_CLIENT,
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          base::SingleThreadTaskRunner::GetCurrentDefault()),
      kTestClientPid);

  mojo::PendingAssociatedReceiver<IPC::mojom::Channel> receiver;
  connection.TakeReceiver(&receiver);

  base::RunLoop run_loop;
  PeerPidReceiver impl(std::move(receiver), run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_EQ(kTestServerPid, impl.peer_pid());

  return 0;
}

TEST_F(IPCMojoBootstrapTest, ReceiveEmptyMessage) {
  base::test::SingleThreadTaskEnvironment task_environment;
  Connection connection(
      IPC::MojoBootstrap::Create(
          helper_.StartChild("IPCMojoBootstrapTestEmptyMessage"),
          IPC::Channel::MODE_SERVER,
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          base::SingleThreadTaskRunner::GetCurrentDefault()),
      kTestServerPid);

  mojo::PendingAssociatedReceiver<IPC::mojom::Channel> receiver;
  connection.TakeReceiver(&receiver);

  base::RunLoop run_loop;
  PeerPidReceiver impl(std::move(receiver), run_loop.QuitClosure(),
                       PeerPidReceiver::MessageExpectation::kExpectedInvalid);
  run_loop.Run();

  // Wait for the Channel to be disconnected so we can reasonably assert that
  // the child's empty message must have been received before we pass the test.
  impl.RunUntilDisconnect();

  EXPECT_TRUE(helper_.WaitForChildTestShutdown());
}

// A long running process that connects to us.
MULTIPROCESS_TEST_MAIN_WITH_SETUP(
    IPCMojoBootstrapTestEmptyMessageTestChildMain,
    ::mojo::core::test::MultiprocessTestHelper::ChildSetup) {
  base::test::SingleThreadTaskEnvironment task_environment;
  Connection connection(
      IPC::MojoBootstrap::Create(
          std::move(mojo::core::test::MultiprocessTestHelper::primordial_pipe),
          IPC::Channel::MODE_CLIENT,
          base::SingleThreadTaskRunner::GetCurrentDefault(),
          base::SingleThreadTaskRunner::GetCurrentDefault()),
      kTestClientPid);

  mojo::PendingAssociatedReceiver<IPC::mojom::Channel> receiver;
  connection.TakeReceiver(&receiver);
  auto& sender = connection.GetSender();

  uint8_t data = 0;
  sender->Receive(
      IPC::MessageView(base::make_span(&data, 0u), std::nullopt /* handles */));

  base::RunLoop run_loop;
  PeerPidReceiver impl(std::move(receiver), run_loop.QuitClosure());
  run_loop.Run();

  return 0;
}

}  // namespace
