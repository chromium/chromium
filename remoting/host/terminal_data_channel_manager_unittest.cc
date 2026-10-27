// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/terminal_data_channel_manager.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "remoting/host/fake_terminal_session.h"
#include "remoting/host/terminal_error.h"
#include "remoting/host/terminal_session_manager.h"
#include "remoting/proto/terminal.pb.h"
#include "remoting/protocol/fake_message_pipe.h"
#include "remoting/protocol/fake_message_pipe_wrapper.h"
#include "remoting/protocol/message_pipe.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {

using AgentInfo = protocol::TerminalControl::AgentInfo;

// Returns the last message sent on `pipe`, parsed as a TerminalControl.
protocol::TerminalControl LastSentMessage(protocol::FakeMessagePipe& pipe) {
  protocol::TerminalControl message;
  if (!pipe.sent_messages().empty()) {
    message.ParseFromString(pipe.sent_messages().back());
  }
  return message;
}

}  // namespace

class TerminalDataChannelManagerTest : public testing::Test {
 protected:
  void SetUp() override {
    FakeTerminalSession::ResetStaticState();
    manager_ = std::make_unique<TerminalDataChannelManager>(base::BindRepeating(
        &TerminalDataChannelManagerTest::CreateOutgoingChannel,
        base::Unretained(this)));
  }

  void TearDown() override {
    manager_.reset();
    // Let any channels scheduled for deletion be deleted.
    FlushMainThread();
  }

  // Runs all tasks that are already queued on the main thread.
  void FlushMainThread() {
    base::test::TestFuture<void> future;
    task_environment_.GetMainThreadTaskRunner()->PostTask(FROM_HERE,
                                                          future.GetCallback());
    ASSERT_TRUE(future.Wait());
  }

  // Starts the manager and waits for persistent terminals to be restored.
  void StartManager() {
    manager_->Start();
    // Flush the ThreadPool task that fetches the persistent terminal IDs, then
    // wait for its reply, which is queued on the main thread.
    base::ThreadPoolInstance::Get()->FlushForTesting();
    FlushMainThread();
  }

  // Simulates the client opening a terminal data channel backed by `pipe`.
  void OpenIncomingChannel(protocol::FakeMessagePipe& pipe) {
    manager_->OnIncomingChannel("terminal-client-1", pipe.Wrap());
    pipe.OpenPipe();
  }

  std::unique_ptr<protocol::MessagePipe> CreateOutgoingChannel(
      const std::string& name) {
    if (!allow_outgoing_channels_) {
      return nullptr;
    }
    auto pipe = std::make_unique<protocol::FakeMessagePipe>(
        /*asynchronous=*/false);
    std::unique_ptr<protocol::MessagePipe> wrapper = pipe->Wrap();
    outgoing_channels_[name] = std::move(pipe);
    return wrapper;
  }

  base::test::TaskEnvironment task_environment_;
  std::map<std::string, std::unique_ptr<protocol::FakeMessagePipe>>
      outgoing_channels_;
  bool allow_outgoing_channels_ = true;
  std::unique_ptr<TerminalDataChannelManager> manager_;
};

TEST_F(TerminalDataChannelManagerTest, CreateTerminal) {
  StartManager();

  protocol::FakeMessagePipe pipe(false);
  OpenIncomingChannel(pipe);

  protocol::TerminalControl response = LastSentMessage(pipe);
  ASSERT_TRUE(response.has_create_response());
  EXPECT_EQ(response.create_response().terminal_id(), 1);
  EXPECT_FALSE(response.create_response().has_error());
}

TEST_F(TerminalDataChannelManagerTest, CreateTerminalFailure) {
  StartManager();

  const TerminalError error(FROM_HERE, TerminalError::Reason::kPtyError,
                            "start failed", 42);
  FakeTerminalSession::SetNextStartError(error);

  // The failure is reported synchronously, before the pipe has opened, so the
  // error must be queued until it does.
  protocol::FakeMessagePipe pipe(false);
  OpenIncomingChannel(pipe);

  protocol::TerminalControl response = LastSentMessage(pipe);
  ASSERT_TRUE(response.has_create_response());
  EXPECT_FALSE(response.create_response().has_terminal_id());
  ASSERT_TRUE(response.create_response().has_error());
  const auto& error_proto = response.create_response().error();
  EXPECT_EQ(
      error_proto.reason(),
      protocol::TerminalControl::CreateTerminalResponse::Error::PTY_ERROR);
  EXPECT_EQ(error_proto.code(), 42);
  EXPECT_EQ(error_proto.error_message(), "start failed");
  EXPECT_EQ(error_proto.file_name(), error.location.file_name());
  EXPECT_EQ(error_proto.line_number(),
            static_cast<uint32_t>(error.location.line_number()));

  // The channel is deleted once the error has been sent.
  EXPECT_TRUE(base::test::RunUntil([&pipe] { return !pipe.HasWrappers(); }));
  EXPECT_TRUE(base::test::RunUntil(
      [] { return FakeTerminalSession::GetActiveSessions().empty(); }));
}

TEST_F(TerminalDataChannelManagerTest, CreateTerminalWaitsForAsyncStart) {
  StartManager();
  FakeTerminalSession::SetDeferStart(true);

  protocol::FakeMessagePipe pipe(false);
  OpenIncomingChannel(pipe);

  ASSERT_EQ(FakeTerminalSession::GetActiveSessions().size(), 1u);
  auto session = FakeTerminalSession::GetActiveSessions().front();
  ASSERT_TRUE(session->has_pending_start());
  EXPECT_TRUE(pipe.sent_messages().empty());

  session->CompleteStart(base::ok());

  protocol::TerminalControl response = LastSentMessage(pipe);
  ASSERT_TRUE(response.has_create_response());
  EXPECT_EQ(response.create_response().terminal_id(), 1);
  EXPECT_FALSE(response.create_response().has_error());
}

TEST_F(TerminalDataChannelManagerTest, CreateTerminalAsyncFailure) {
  StartManager();
  FakeTerminalSession::SetDeferStart(true);

  protocol::FakeMessagePipe pipe(false);
  OpenIncomingChannel(pipe);

  ASSERT_EQ(FakeTerminalSession::GetActiveSessions().size(), 1u);
  auto session = FakeTerminalSession::GetActiveSessions().front();
  ASSERT_TRUE(session->has_pending_start());

  session->CompleteStart(base::unexpected(TerminalError(
      FROM_HERE, TerminalError::Reason::kLaunchFailed, "launch failed")));

  protocol::TerminalControl response = LastSentMessage(pipe);
  ASSERT_TRUE(response.has_create_response());
  EXPECT_FALSE(response.create_response().has_terminal_id());
  ASSERT_TRUE(response.create_response().has_error());
  EXPECT_EQ(
      response.create_response().error().reason(),
      protocol::TerminalControl::CreateTerminalResponse::Error::LAUNCH_FAILED);
  EXPECT_EQ(response.create_response().error().error_message(),
            "launch failed");
  EXPECT_TRUE(base::test::RunUntil([&pipe] { return !pipe.HasWrappers(); }));
  EXPECT_TRUE(base::test::RunUntil(
      [] { return FakeTerminalSession::GetActiveSessions().empty(); }));
}

TEST_F(TerminalDataChannelManagerTest, DestroyedWhileCreatePending) {
  StartManager();
  FakeTerminalSession::SetDeferStart(true);

  protocol::FakeMessagePipe pipe(false);
  OpenIncomingChannel(pipe);
  ASSERT_EQ(FakeTerminalSession::GetActiveSessions().size(), 1u);

  manager_.reset();
  EXPECT_TRUE(pipe.sent_messages().empty());
  EXPECT_TRUE(FakeTerminalSession::GetActiveSessions().empty());
}

TEST_F(TerminalDataChannelManagerTest, ChannelClosedWhileCreatePending) {
  StartManager();
  FakeTerminalSession::SetDeferStart(true);

  protocol::FakeMessagePipe pipe(false);
  OpenIncomingChannel(pipe);
  ASSERT_EQ(FakeTerminalSession::GetActiveSessions().size(), 1u);
  auto session = FakeTerminalSession::GetActiveSessions().front();

  pipe.ClosePipe();
  session->CompleteStart(base::ok());

  // The terminal is left running so that it can be restored on reconnection.
  EXPECT_TRUE(pipe.sent_messages().empty());
  EXPECT_FALSE(FakeTerminalSession::WasTerminated(1));
  EXPECT_NE(
      manager_->terminal_session_manager_for_tests()->GetTerminalSession(1),
      nullptr);
}

TEST_F(TerminalDataChannelManagerTest,
       MessagesReceivedWhileCreatePendingAreDelivered) {
  StartManager();
  FakeTerminalSession::SetDeferStart(true);

  protocol::FakeMessagePipe pipe(false);
  OpenIncomingChannel(pipe);
  ASSERT_EQ(FakeTerminalSession::GetActiveSessions().size(), 1u);
  auto session = FakeTerminalSession::GetActiveSessions().front();

  // The client sends input and a resize before the terminal has started.
  protocol::TerminalControl resize_msg;
  resize_msg.mutable_resize_terminal()->set_width(80);
  resize_msg.mutable_resize_terminal()->set_height(24);
  pipe.ReceiveProtobufMessage(resize_msg);
  protocol::TerminalControl input_msg;
  input_msg.mutable_terminal_input()->set_input("early input");
  pipe.ReceiveProtobufMessage(input_msg);
  EXPECT_TRUE(session->inputs().empty());
  EXPECT_TRUE(session->resizes().empty());

  session->CompleteStart(base::ok());

  // The response is sent first, then the queued messages are delivered in
  // order.
  protocol::TerminalControl response = LastSentMessage(pipe);
  ASSERT_TRUE(response.has_create_response());
  EXPECT_EQ(response.create_response().terminal_id(), 1);
  ASSERT_EQ(session->resizes().size(), 1u);
  EXPECT_EQ(session->resizes()[0].first, 80u);
  EXPECT_EQ(session->resizes()[0].second, 24u);
  ASSERT_EQ(session->inputs().size(), 1u);
  EXPECT_EQ(session->inputs()[0], "early input");
}

TEST_F(TerminalDataChannelManagerTest,
       MessagesReceivedWhileCreatePendingAreLimited) {
  // Must match kMaxPendingIncomingMessages in terminal_data_channel_manager.cc.
  constexpr size_t kMaxPendingIncomingMessages = 50;

  StartManager();
  FakeTerminalSession::SetDeferStart(true);

  protocol::FakeMessagePipe pipe(false);
  OpenIncomingChannel(pipe);
  ASSERT_EQ(FakeTerminalSession::GetActiveSessions().size(), 1u);
  auto session = FakeTerminalSession::GetActiveSessions().front();

  // Send one more message than can be queued before the terminal starts.
  for (size_t i = 0; i <= kMaxPendingIncomingMessages; ++i) {
    protocol::TerminalControl input_msg;
    input_msg.mutable_terminal_input()->set_input(base::NumberToString(i));
    pipe.ReceiveProtobufMessage(input_msg);
  }

  session->CompleteStart(base::ok());

  // Only the first kMaxPendingIncomingMessages are delivered, in order.
  ASSERT_EQ(session->inputs().size(), kMaxPendingIncomingMessages);
  for (size_t i = 0; i < kMaxPendingIncomingMessages; ++i) {
    EXPECT_EQ(session->inputs()[i], base::NumberToString(i));
  }

  // Messages received after the terminal has started are not limited.
  protocol::TerminalControl input_msg;
  input_msg.mutable_terminal_input()->set_input("later");
  pipe.ReceiveProtobufMessage(input_msg);
  ASSERT_EQ(session->inputs().size(), kMaxPendingIncomingMessages + 1);
  EXPECT_EQ(session->inputs().back(), "later");
}

TEST_F(TerminalDataChannelManagerTest, TerminalOutput) {
  StartManager();

  protocol::FakeMessagePipe pipe(false);
  OpenIncomingChannel(pipe);
  ASSERT_FALSE(FakeTerminalSession::GetActiveSessions().empty());
  auto* session = FakeTerminalSession::GetActiveSessions().front().get();

  session->TriggerOutput("test output");

  ASSERT_GE(pipe.sent_messages().size(), 2u);
  protocol::TerminalControl message = LastSentMessage(pipe);
  ASSERT_TRUE(message.has_terminal_output());
  EXPECT_EQ(message.terminal_output().output(), "test output");
}

TEST_F(TerminalDataChannelManagerTest, TerminalInputAndResize) {
  StartManager();

  protocol::FakeMessagePipe pipe(false);
  OpenIncomingChannel(pipe);
  ASSERT_FALSE(FakeTerminalSession::GetActiveSessions().empty());
  auto* session = FakeTerminalSession::GetActiveSessions().front().get();

  protocol::TerminalControl input_msg;
  input_msg.mutable_terminal_input()->set_input("test input");
  pipe.ReceiveProtobufMessage(input_msg);
  EXPECT_EQ(session->inputs().back(), "test input");

  protocol::TerminalControl resize_msg;
  resize_msg.mutable_resize_terminal()->set_width(80);
  resize_msg.mutable_resize_terminal()->set_height(24);
  pipe.ReceiveProtobufMessage(resize_msg);
  auto size = session->resizes().back();
  EXPECT_EQ(size.first, 80u);
  EXPECT_EQ(size.second, 24u);
}

TEST_F(TerminalDataChannelManagerTest, ProcessInfo) {
  StartManager();

  protocol::FakeMessagePipe pipe(false);
  OpenIncomingChannel(pipe);
  ASSERT_FALSE(FakeTerminalSession::GetActiveSessions().empty());
  auto* session = FakeTerminalSession::GetActiveSessions().front().get();

  session->TriggerProcessInfo(true, "vim");

  ASSERT_GE(pipe.sent_messages().size(), 2u);
  protocol::TerminalControl message = LastSentMessage(pipe);
  ASSERT_TRUE(message.has_process_info());
  EXPECT_EQ(message.process_info().process_name(), "vim");
  EXPECT_TRUE(message.process_info().is_active());
}

TEST_F(TerminalDataChannelManagerTest, RemoveRequest) {
  StartManager();

  protocol::FakeMessagePipe pipe(false);
  OpenIncomingChannel(pipe);
  ASSERT_FALSE(FakeTerminalSession::GetActiveSessions().empty());

  protocol::TerminalControl remove_msg;
  remove_msg.mutable_remove_request();
  pipe.ReceiveProtobufMessage(remove_msg);

  EXPECT_TRUE(FakeTerminalSession::GetActiveSessions().empty());
}

TEST_F(TerminalDataChannelManagerTest, TerminalExited) {
  StartManager();

  protocol::FakeMessagePipe pipe(false);
  OpenIncomingChannel(pipe);
  ASSERT_FALSE(FakeTerminalSession::GetActiveSessions().empty());
  auto* session = FakeTerminalSession::GetActiveSessions().front().get();

  session->TriggerExit();

  ASSERT_GE(pipe.sent_messages().size(), 2u);
  EXPECT_TRUE(LastSentMessage(pipe).has_close_terminal());
}

TEST_F(TerminalDataChannelManagerTest, RestoreTerminals) {
  FakeTerminalSession::SetPersistentTerminalIds({1, 2});
  StartManager();

  ASSERT_EQ(outgoing_channels_.size(), 2u);
  protocol::FakeMessagePipe* pipe1 = outgoing_channels_["terminal-1"].get();
  protocol::FakeMessagePipe* pipe2 = outgoing_channels_["terminal-2"].get();
  ASSERT_NE(pipe1, nullptr);
  ASSERT_NE(pipe2, nullptr);

  pipe1->OpenPipe();
  pipe2->OpenPipe();

  protocol::TerminalControl response1 = LastSentMessage(*pipe1);
  ASSERT_TRUE(response1.has_create_response());
  EXPECT_EQ(response1.create_response().terminal_id(), 1);

  protocol::TerminalControl response2 = LastSentMessage(*pipe2);
  ASSERT_TRUE(response2.has_create_response());
  EXPECT_EQ(response2.create_response().terminal_id(), 2);
}

TEST_F(TerminalDataChannelManagerTest,
       RestoreTerminalsWhenChannelCannotBeCreated) {
  FakeTerminalSession::SetPersistentTerminalIds({1});
  allow_outgoing_channels_ = false;
  StartManager();

  // The terminal is still restored, even though there is no channel for it.
  EXPECT_TRUE(outgoing_channels_.empty());
  EXPECT_NE(
      manager_->terminal_session_manager_for_tests()->GetTerminalSession(1),
      nullptr);
}

TEST_F(TerminalDataChannelManagerTest, AgentStatus) {
  StartManager();

  // The ID assigned to the first terminal created by the manager.
  constexpr int32_t kTerminalId = 1;
  // An ID that does not correspond to any terminal.
  constexpr int32_t kUnknownTerminalId = 99;

  AgentInfo status;
  status.set_agent_state(AgentInfo::TOOL_USE);
  status.set_conversation_title("My Title");

  // Ignored before terminal 1 is created.
  manager_->SetAgentStatus(kTerminalId, status);

  protocol::FakeMessagePipe pipe(false);
  OpenIncomingChannel(pipe);
  ASSERT_EQ(pipe.sent_messages().size(), 1u);

  // Ignored for an unknown terminal ID.
  manager_->SetAgentStatus(kUnknownTerminalId, status);
  EXPECT_EQ(pipe.sent_messages().size(), 1u);

  // Ignored when agent_state is UNSPECIFIED and conversation_title is unset.
  AgentInfo empty_status;
  empty_status.set_agent_state(AgentInfo::UNSPECIFIED);
  manager_->SetAgentStatus(kTerminalId, empty_status);
  EXPECT_EQ(pipe.sent_messages().size(), 1u);

  // Forwarded when both agent_state and conversation_title are set.
  manager_->SetAgentStatus(kTerminalId, status);
  ASSERT_EQ(pipe.sent_messages().size(), 2u);
  protocol::TerminalControl message = LastSentMessage(pipe);
  ASSERT_TRUE(message.has_agent_info());
  EXPECT_EQ(message.agent_info().agent_state(), AgentInfo::TOOL_USE);
  EXPECT_EQ(message.agent_info().conversation_title(), "My Title");

  // Forwarded when only agent_state is set.
  AgentInfo state_only;
  state_only.set_agent_state(AgentInfo::WORKING);
  manager_->SetAgentStatus(kTerminalId, state_only);
  ASSERT_EQ(pipe.sent_messages().size(), 3u);
  message = LastSentMessage(pipe);
  ASSERT_TRUE(message.has_agent_info());
  EXPECT_EQ(message.agent_info().agent_state(), AgentInfo::WORKING);
  EXPECT_FALSE(message.agent_info().has_conversation_title());

  // Forwarded when only conversation_title is set.
  AgentInfo title_only;
  title_only.set_conversation_title("Title Only");
  manager_->SetAgentStatus(kTerminalId, title_only);
  ASSERT_EQ(pipe.sent_messages().size(), 4u);
  message = LastSentMessage(pipe);
  ASSERT_TRUE(message.has_agent_info());
  EXPECT_FALSE(message.agent_info().has_agent_state());
  EXPECT_EQ(message.agent_info().conversation_title(), "Title Only");

  // Ignored after terminal 1 is closed.
  protocol::TerminalControl remove_msg;
  remove_msg.mutable_remove_request();
  pipe.ReceiveProtobufMessage(remove_msg);
  manager_->SetAgentStatus(kTerminalId, status);
  EXPECT_EQ(pipe.sent_messages().size(), 4u);
}

}  // namespace remoting
