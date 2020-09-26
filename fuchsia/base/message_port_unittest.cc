// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <lib/fidl/cpp/binding.h>

#include "base/barrier_closure.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "fuchsia/base/frame_test_util.h"
#include "fuchsia/base/mem_buffer_util.h"
#include "fuchsia/base/message_port.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cr_fuchsia {
namespace {

using WebMessage = fuchsia::web::WebMessage;

class MessagePortTest : public testing::Test {
 public:
  MessagePortTest() = default;
  ~MessagePortTest() override = default;

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::IO};
};

class TestFidlMessageReceiver {
 public:
  explicit TestFidlMessageReceiver(fuchsia::web::MessagePortPtr message_port)
      : port_(std::move(message_port)) {
    port_.set_error_handler([this](zx_status_t status) {
      ASSERT_EQ(ZX_ERR_PEER_CLOSED, status);
      std::move(on_error_).Run();
    });
    ReadNextMessage();
  }

  ~TestFidlMessageReceiver() = default;

  TestFidlMessageReceiver(const TestFidlMessageReceiver&) = delete;
  TestFidlMessageReceiver& operator=(const TestFidlMessageReceiver&) = delete;

  // Spins a runloop until the message buffer contains at least |num_messages|.
  // Returns immediately if the buffer already has the requisite number.
  void RunUntilMessageCountEquals(size_t num_messages) {
    if (messages_.size() >= num_messages)
      return;

    base::RunLoop run_loop;
    on_message_ = base::BarrierClosure(num_messages - messages_.size(),
                                       run_loop.QuitClosure());
    run_loop.Run();
  }

  // Spins a runloop until the underlying FIDL WebMessagePort is disconnected.
  void RunUntilError() {
    base::RunLoop run_loop;
    on_error_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  std::vector<WebMessage>& messages() { return messages_; }

 private:
  void ReadNextMessage() {
    port_->ReceiveMessage([this](WebMessage message) {
      messages_.push_back(std::move(message));
      on_message_.Run();
      ReadNextMessage();
    });
  }

  std::vector<WebMessage> messages_;
  base::RepeatingClosure on_message_;
  base::OnceClosure on_error_;
  fuchsia::web::MessagePortPtr port_;
};

// Sends data through adapted MessagePort, exercising the following:
// * Sending via FIDL
// * Receiving via Blink
// * Sending via Blink
// * Receiving via FIDL
TEST_F(MessagePortTest, Roundtrip) {
  fuchsia::web::MessagePortPtr fidl_port;
  blink::WebMessagePort blink_port =
      cr_fuchsia::BlinkMessagePortFromFidl(fidl_port.NewRequest());
  TestFidlMessageReceiver receiver(
      cr_fuchsia::FidlMessagePortFromBlink(std::move(blink_port)).Bind());

  const std::vector<std::string> messages = {"foo", "bar", "baz"};
  for (const auto& message : messages) {
    WebMessage fidl_message;
    fidl_message.set_data(cr_fuchsia::MemBufferFromString(message, "test"));
    fidl_port->PostMessage(std::move(fidl_message),
                           [](fuchsia::web::MessagePort_PostMessage_Result) {});
  }

  receiver.RunUntilMessageCountEquals(messages.size());
  for (size_t i = 0; i < messages.size(); ++i) {
    std::string data;
    ASSERT_TRUE(
        cr_fuchsia::StringFromMemBuffer(receiver.messages()[i].data(), &data));
    EXPECT_EQ(data, messages[i]);
  }

  fidl_port.Unbind();
  receiver.RunUntilError();
}

// Transfers message ports over message channels through multiple layers of
// recursion.
TEST_F(MessagePortTest, RoundtripWithPorts) {
  constexpr int kNestingLevel = 5;

  fuchsia::web::MessagePortPtr fidl_port;
  for (int i = 0; i < kNestingLevel; ++i) {
    blink::WebMessagePort blink_port =
        cr_fuchsia::BlinkMessagePortFromFidl(fidl_port.NewRequest());
    TestFidlMessageReceiver receiver(
        cr_fuchsia::FidlMessagePortFromBlink(std::move(blink_port)).Bind());

    fuchsia::web::MessagePortPtr transferred_port;
    constexpr char kData[] = "lore";
    fidl_port->PostMessage(cr_fuchsia::CreateWebMessageWithMessagePortRequest(
                               transferred_port.NewRequest(),
                               cr_fuchsia::MemBufferFromString(kData, "test")),
                           [](fuchsia::web::MessagePort_PostMessage_Result) {});
    receiver.RunUntilMessageCountEquals(1);

    std::string data;
    ASSERT_TRUE(
        cr_fuchsia::StringFromMemBuffer(receiver.messages()[0].data(), &data));
    EXPECT_EQ(data, kData);

    // Drop the previous MessagePort, verify that the channel error was
    // propagated correctly, and select the newest MessagePort
    // for the next roundtrip iteration.
    fidl_port = std::move(transferred_port);
    receiver.RunUntilError();
  }
}

class NullReceiver : public blink::WebMessagePort::MessageReceiver {
  bool OnMessage(blink::WebMessagePort::Message message) override {
    NOTREACHED();
    return false;
  }
  void OnPipeError() override { NOTREACHED(); }
};

// Counts PostMessage calls from a FIDL client, with pause and resume methods to
// manage channel backpressure.
class TestFidlMessagePortCountingSink : public fuchsia::web::MessagePort {
 public:
  TestFidlMessagePortCountingSink() : binding_(this) {}
  ~TestFidlMessagePortCountingSink() override = default;

  TestFidlMessagePortCountingSink(const TestFidlMessagePortCountingSink&) =
      delete;
  TestFidlMessagePortCountingSink& operator=(
      const TestFidlMessagePortCountingSink&) = delete;

  fidl::InterfaceHandle<fuchsia::web::MessagePort> GetClient() {
    return binding_.NewBinding();
  }

  // Stops acknowledging calls to PostMessage until ResumeAck() is invoked.
  void PauseAck() { pause_ack_ = true; }

  // Resumes acknowledging PostMessage calls.
  void ResumeAck() {
    DCHECK(pause_ack_);
    pause_ack_ = false;

    if (post_message_callback_)
      AckPostMessage();
  }

  // Spins a runloop until the message buffer contains at least |num_messages|.
  // Returns immediately if the buffer already has that number of messages.
  void RunUntilMessageCountEquals(size_t num_messages) {
    if (message_count_ >= num_messages)
      return;

    base::RunLoop run_loop;
    on_message_ = base::BarrierClosure(num_messages - message_count_,
                                       run_loop.QuitClosure());
    run_loop.Run();
  }

  size_t message_count() const { return message_count_; }

 private:
  void AckPostMessage() {
    ASSERT_TRUE(post_message_callback_);
    DCHECK(!pause_ack_);

    fuchsia::web::MessagePort_PostMessage_Result result;
    result.set_response(fuchsia::web::MessagePort_PostMessage_Response());
    (*post_message_callback_)(std::move(result));
    post_message_callback_ = {};
  }

  void PostMessage(fuchsia::web::WebMessage message,
                   PostMessageCallback callback) override {
    ++message_count_;

    post_message_callback_ = std::move(callback);

    if (!pause_ack_)
      AckPostMessage();

    if (on_message_)
      on_message_->Run();
  }

  void ReceiveMessage(ReceiveMessageCallback callback) override {}

  size_t message_count_ = 0;
  bool pause_ack_ = false;
  base::Optional<base::RepeatingClosure> on_message_;
  fidl::Binding<fuchsia::web::MessagePort> binding_;
  base::Optional<PostMessageCallback> post_message_callback_;
};

// Sends a burst of five messages over Blink (which doesn't use a flow
// controlled interface), and verify that the adapter respects FIDL flow
// control.
TEST_F(MessagePortTest, BlinkMessageBurstForClientAdapter) {
  TestFidlMessagePortCountingSink fidl_sink;
  blink::WebMessagePort blink_port =
      cr_fuchsia::BlinkMessagePortFromFidl(fidl_sink.GetClient());

  fidl_sink.PauseAck();
  NullReceiver blink_receiver;
  blink_port.SetReceiver(&blink_receiver, base::ThreadTaskRunnerHandle::Get());
  for (int i = 0; i < 5; ++i) {
    blink::WebMessagePort::Message blink_message(base::UTF8ToUTF16("test"));
    ASSERT_TRUE(blink_port.PostMessage(std::move(blink_message)));
  }
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(fidl_sink.message_count(), 1u);

  fidl_sink.ResumeAck();
  fidl_sink.RunUntilMessageCountEquals(5u);
}

}  // namespace
}  // namespace cr_fuchsia
