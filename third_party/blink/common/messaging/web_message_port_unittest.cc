// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/messaging/web_message_port.h"

#include <string>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

namespace {

using Message = WebMessagePort::Message;

class LenientMockReceiver : public WebMessagePort::MessageReceiver {
 public:
  LenientMockReceiver() = default;
  ~LenientMockReceiver() override = default;

  // WebMessagePort::MessageReceiver implementation:
  MOCK_METHOD1(OnMessage, bool(Message));
  MOCK_METHOD0(OnPipeError, void());
};

using MockReceiver = testing::StrictMock<LenientMockReceiver>;

using testing::_;
using testing::Invoke;

}  // namespace

TEST(WebMessagePortTest, EndToEnd) {
  base::test::SingleThreadTaskEnvironment task_env;

  // Create a dummy pipe and ensure it behaves as expected.
  WebMessagePort port0;
  EXPECT_FALSE(port0.IsValid());
  EXPECT_FALSE(port0.is_errored());
  EXPECT_TRUE(port0.is_closed());
  EXPECT_FALSE(port0.is_transferable());
  EXPECT_FALSE(port0.HasReceiver());
  EXPECT_FALSE(port0.CanPostMessage());

  // Create a pipe.
  auto pipe = WebMessagePort::CreatePair();
  port0 = std::move(pipe.first);
  WebMessagePort port1 = std::move(pipe.second);

  EXPECT_TRUE(port0.IsValid());
  EXPECT_FALSE(port0.is_errored());
  EXPECT_FALSE(port0.is_closed());
  EXPECT_TRUE(port0.is_transferable());
  EXPECT_FALSE(port0.HasReceiver());
  EXPECT_FALSE(port0.CanPostMessage());
  EXPECT_TRUE(port1.IsValid());
  EXPECT_FALSE(port1.is_errored());
  EXPECT_FALSE(port1.is_closed());
  EXPECT_TRUE(port1.is_transferable());
  EXPECT_FALSE(port1.HasReceiver());
  EXPECT_FALSE(port1.CanPostMessage());

  // And bind both endpoints to distinct receivers. The ports should remain
  // valid but no longer be transferable.
  MockReceiver receiver0;
  MockReceiver receiver1;
  port0.SetReceiver(&receiver0, task_env.GetMainThreadTaskRunner());
  port1.SetReceiver(&receiver1, task_env.GetMainThreadTaskRunner());

  EXPECT_TRUE(port0.IsValid());
  EXPECT_FALSE(port0.is_errored());
  EXPECT_FALSE(port0.is_closed());
  EXPECT_FALSE(port0.is_transferable());
  EXPECT_TRUE(port0.HasReceiver());
  EXPECT_TRUE(port0.CanPostMessage());
  EXPECT_TRUE(port1.IsValid());
  EXPECT_FALSE(port1.is_errored());
  EXPECT_FALSE(port1.is_closed());
  EXPECT_FALSE(port1.is_transferable());
  EXPECT_TRUE(port1.HasReceiver());
  EXPECT_TRUE(port1.CanPostMessage());

  // Send a simple string-only message one way from port 0 to port 1.
  std::u16string message(u"foo");
  {
    base::RunLoop run_loop;
    EXPECT_CALL(receiver1, OnMessage(_))
        .WillOnce(
            Invoke([&message, &run_loop](Message&& received_message) -> bool {
              EXPECT_EQ(message, received_message.data);
              EXPECT_TRUE(received_message.ports.empty());
              run_loop.Quit();
              return true;
            }));
    port0.PostMessage(Message(message));
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(&receiver0);
    testing::Mock::VerifyAndClearExpectations(&receiver1);
  }

  // Send a message the other way (from 1 to 0) with a port.
  auto pipe2 = WebMessagePort::CreatePair();
  {
    base::RunLoop run_loop;
    EXPECT_CALL(receiver0, OnMessage(_))
        .WillOnce(
            Invoke([&message, &run_loop](Message&& received_message) -> bool {
              EXPECT_EQ(message, received_message.data);
              EXPECT_EQ(1u, received_message.ports.size());
              run_loop.Quit();
              return true;
            }));
    port1.PostMessage(Message(message, std::move(pipe2.first)));
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(&receiver0);
    testing::Mock::VerifyAndClearExpectations(&receiver1);
  }

  // Close one end of the pipe and expect the other end to get an error.
  {
    base::RunLoop run_loop;
    EXPECT_CALL(receiver1, OnPipeError()).WillOnce(Invoke([&run_loop]() {
      run_loop.Quit();
    }));
    port0.Close();
    EXPECT_FALSE(port0.IsValid());
    EXPECT_FALSE(port0.is_errored());
    EXPECT_TRUE(port0.is_closed());
    EXPECT_FALSE(port0.is_transferable());
    EXPECT_FALSE(port0.HasReceiver());
    EXPECT_FALSE(port0.CanPostMessage());
    run_loop.Run();
    EXPECT_TRUE(port1.IsValid());
    EXPECT_TRUE(port1.is_errored());
    EXPECT_FALSE(port1.is_closed());
    EXPECT_FALSE(port1.is_transferable());
    EXPECT_TRUE(port1.HasReceiver());
    EXPECT_FALSE(port1.CanPostMessage());
    testing::Mock::VerifyAndClearExpectations(&receiver0);
    testing::Mock::VerifyAndClearExpectations(&receiver1);
  }

  // Reset the pipe and expect it to go back to a fully default state.
  port1.Reset();
  EXPECT_FALSE(port1.IsValid());
  EXPECT_FALSE(port1.is_errored());
  EXPECT_TRUE(port1.is_closed());
  EXPECT_FALSE(port1.is_transferable());
  EXPECT_FALSE(port1.HasReceiver());
  EXPECT_FALSE(port1.CanPostMessage());
}

TEST(WebMessagePortTest, MoveAssignToConnectedPort) {
  base::test::SingleThreadTaskEnvironment task_env;

  // Must outlive WebMessagePorts.
  MockReceiver receiver0;
  MockReceiver receiver1;

  // Create a pipe.
  auto pipe = WebMessagePort::CreatePair();
  WebMessagePort port0 = std::move(pipe.first);
  WebMessagePort port1 = std::move(pipe.second);

  // And bind both endpoints to distinct receivers.
  port0.SetReceiver(&receiver0, task_env.GetMainThreadTaskRunner());
  port1.SetReceiver(&receiver1, task_env.GetMainThreadTaskRunner());

  // Move assign a new port into the open one. This should result in the
  // open port being closed, which can be noticed on the remote half as a
  // connection error.
  base::RunLoop run_loop;
  EXPECT_CALL(receiver1, OnPipeError()).WillOnce(Invoke([&run_loop]() {
    run_loop.Quit();
  }));

  port0 = WebMessagePort();

  run_loop.Run();
  testing::Mock::VerifyAndClearExpectations(&receiver0);
  testing::Mock::VerifyAndClearExpectations(&receiver1);
}

}  // namespace blink
