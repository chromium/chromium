// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "base/timer/mock_timer.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/media/router/providers/cast/dual_media_sink_service.h"
#include "chrome/browser/media/router/test/noop_dual_media_sink_service.h"
#include "chrome/browser/ui/browser.h"
#include "components/cast_channel/cast_socket.h"
#include "components/cast_channel/cast_socket_service.h"
#include "components/cast_channel/cast_test_util.h"
#include "components/cast_channel/logger.h"
#include "components/cast_channel/proto/cast_channel.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/api/cast_channel/cast_channel_api.h"
#include "extensions/common/api/cast_channel.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/switches.h"
#include "extensions/test/extension_test_message_listener.h"
#include "extensions/test/result_catcher.h"
#include "net/base/completion_callback.h"
#include "net/base/ip_address.h"
#include "net/base/net_errors.h"
#include "net/log/test_net_log.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gmock_mutant.h"

using ::cast_channel::CastMessage;
using ::cast_channel::CastSocket;
using ::cast_channel::CastTransport;
using ::cast_channel::ChannelError;
using ::cast_channel::CreateIPEndPointForTest;
using ::cast_channel::LastError;
using ::cast_channel::Logger;
using ::cast_channel::MockCastSocket;
using ::cast_channel::MockCastTransport;
using ::cast_channel::ReadyState;
using extensions::api::cast_channel::ErrorInfo;
using extensions::api::cast_channel::MessageInfo;
using extensions::Extension;

namespace utils = extension_function_test_utils;

using ::testing::_;
using ::testing::A;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::InSequence;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::ReturnPointee;
using ::testing::SaveArg;
using ::testing::WithArgs;

namespace {

const char kTestExtensionId[] = "ddchlicdkolnonkihahngkmmmjnjlkkf";

static void FillCastMessage(const std::string& message,
                            CastMessage* cast_message) {
  cast_message->set_namespace_("foo");
  cast_message->set_source_id("src");
  cast_message->set_destination_id("dest");
  cast_message->set_payload_utf8(message);
  cast_message->set_payload_type(CastMessage::STRING);
}

ACTION_TEMPLATE(InvokeCompletionCallback,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(result)) {
  std::get<k>(args).Run(result);
}

}  // namespace

class CastChannelAPITest : public extensions::ExtensionApiTest {
 public:
  CastChannelAPITest() : ip_endpoint_(CreateIPEndPointForTest()) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    extensions::ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        extensions::switches::kWhitelistedExtensionID, kTestExtensionId);
  }

  void SetUp() override {
    // Stub out DualMediaSinkService so it does not interfere with the test.
    media_router::DualMediaSinkService::SetInstanceForTest(
        new media_router::NoopDualMediaSinkService());
    extensions::ExtensionApiTest::SetUp();
  }

  void SetUpMockCastSocket() {
    extensions::CastChannelAPI* api = GetApi();

    net::IPEndPoint ip_endpoint(net::IPAddress(192, 168, 1, 1), 8009);
    mock_cast_socket_ = new MockCastSocket();
    mock_cast_socket_->SetIPEndpoint(ip_endpoint_);
    mock_cast_socket_->SetKeepAlive(false);
    // Transfers ownership of the socket.
    api->SetSocketForTest(base::WrapUnique<CastSocket>(mock_cast_socket_));
  }

  void SetUpOpenSendClose() {
    SetUpMockCastSocket();
    mock_cast_socket_->SetErrorState(ChannelError::NONE);
    {
      InSequence sequence;

      EXPECT_CALL(*mock_cast_socket_, ConnectInternal(_))
          .WillOnce(WithArgs<0>(
              Invoke([&](const MockCastSocket::MockOnOpenCallback& callback) {
                callback.Run(mock_cast_socket_);
              })));
      EXPECT_CALL(*mock_cast_socket_, ready_state())
          .WillRepeatedly(Return(ReadyState::OPEN));
      EXPECT_CALL(*mock_cast_socket_->mock_transport(),
                  SendMessage(A<const CastMessage&>(), _))
          .WillOnce(InvokeCompletionCallback<1>(net::OK));
      EXPECT_CALL(*mock_cast_socket_, ready_state())
          .WillOnce(Return(ReadyState::OPEN));
      EXPECT_CALL(*mock_cast_socket_, Close(_))
          .WillOnce(InvokeCompletionCallback<0>(net::OK));
      EXPECT_CALL(*mock_cast_socket_, ready_state())
          .WillOnce(Return(ReadyState::CLOSED));
    }
  }

  void SetUpOpenErrorSend() {
    SetUpMockCastSocket();
    mock_cast_socket_->SetErrorState(ChannelError::CONNECT_ERROR);
    {
      InSequence sequence;

      EXPECT_CALL(*mock_cast_socket_, ConnectInternal(_))
          .WillOnce(WithArgs<0>(
              Invoke([&](const MockCastSocket::MockOnOpenCallback& callback) {
                callback.Run(mock_cast_socket_);
              })));
      EXPECT_CALL(*mock_cast_socket_, ready_state())
          .WillRepeatedly(Return(ReadyState::CLOSED));
      EXPECT_CALL(*mock_cast_socket_->mock_transport(), SendMessage(_, _))
          .Times(0);
      EXPECT_CALL(*mock_cast_socket_, Close(_))
          .WillOnce(InvokeCompletionCallback<0>(net::OK));
      EXPECT_CALL(*mock_cast_socket_, ready_state())
          .WillOnce(Return(ReadyState::CLOSED));
    }
  }

  void SetUpOpenPingTimeout() {
    SetUpMockCastSocket();
    mock_cast_socket_->SetErrorState(ChannelError::NONE);
    mock_cast_socket_->SetKeepAlive(true);
    {
      InSequence sequence;
      EXPECT_CALL(*mock_cast_socket_, ConnectInternal(_))
          .WillOnce(WithArgs<0>(
              Invoke([&](const MockCastSocket::MockOnOpenCallback& callback) {
                callback.Run(mock_cast_socket_);
              })));
      EXPECT_CALL(*mock_cast_socket_, ready_state())
          .WillOnce(Return(ReadyState::OPEN))
          .RetiresOnSaturation();
      EXPECT_CALL(*mock_cast_socket_, AddObserver(_))
          .WillOnce(SaveArg<0>(&message_observer_));
      EXPECT_CALL(*mock_cast_socket_, ready_state())
          .Times(2)
          .WillRepeatedly(Return(ReadyState::CLOSED));
    }
    EXPECT_CALL(*mock_cast_socket_, Close(_))
        .WillOnce(InvokeCompletionCallback<0>(net::OK));
  }

  extensions::CastChannelAPI* GetApi() {
    return extensions::CastChannelAPI::Get(profile());
  }

  cast_channel::CastSocketService* GetCastSocketService() {
    return cast_channel::CastSocketService::GetInstance();
  }

  // Logs some bogus error details and calls the OnError handler.
  void DoCallOnError(cast_channel::CastSocketService* cast_socket_service) {
    cast_socket_service->GetLogger()->LogSocketEventWithRv(
        mock_cast_socket_->id(), cast_channel::ChannelEvent::SOCKET_WRITE,
        net::ERR_FAILED);
    message_observer_->OnError(*mock_cast_socket_, ChannelError::CONNECT_ERROR);
  }

 protected:
  void CallOnMessage(const std::string& message) {
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::IO},
        base::Bind(&CastChannelAPITest::DoCallOnMessage, base::Unretained(this),
                   GetApi(), mock_cast_socket_, message));
  }

  void DoCallOnMessage(extensions::CastChannelAPI* api,
                       MockCastSocket* cast_socket,
                       const std::string& message) {
    CastMessage cast_message;
    FillCastMessage(message, &cast_message);
    message_observer_->OnMessage(*cast_socket, cast_message);
  }

  // Fires a timer on the IO thread.
  void FireTimeout() {
    base::PostTaskWithTraits(
        FROM_HERE, {content::BrowserThread::IO},
        base::Bind(&CastChannelAPITest::DoFireTimeout, base::Unretained(this),
                   mock_cast_socket_));
  }

  void DoFireTimeout(MockCastSocket* cast_socket) {
    message_observer_->OnError(*cast_socket,
                               cast_channel::ChannelError::PING_TIMEOUT);
  }

  extensions::CastChannelOpenFunction* CreateOpenFunction(
      scoped_refptr<const Extension> extension) {
    extensions::CastChannelOpenFunction* cast_channel_open_function =
      new extensions::CastChannelOpenFunction;
    cast_channel_open_function->set_extension(extension.get());
    return cast_channel_open_function;
  }

  extensions::CastChannelSendFunction* CreateSendFunction(
      scoped_refptr<const Extension> extension) {
    extensions::CastChannelSendFunction* cast_channel_send_function =
      new extensions::CastChannelSendFunction;
    cast_channel_send_function->set_extension(extension.get());
    return cast_channel_send_function;
  }

  MockCastSocket* mock_cast_socket_;
  net::IPEndPoint ip_endpoint_;
  LastError last_error_;
  CastSocket::Observer* message_observer_;
  net::TestNetLog capturing_net_log_;
  int channel_id_;
};

ACTION_P2(InvokeObserverOnError, api_test, cast_socket_service) {
  base::PostTaskWithTraits(
      FROM_HERE, {content::BrowserThread::IO},
      base::Bind(&CastChannelAPITest::DoCallOnError, base::Unretained(api_test),
                 base::Unretained(cast_socket_service)));
}

// TODO(kmarshall): Win Dbg has a workaround that makes RunExtensionSubtest
// always return true without actually running the test. Remove when fixed.
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_TestOpenSendClose DISABLED_TestOpenSendClose
#else
#define MAYBE_TestOpenSendClose TestOpenSendClose
#endif
// Test loading extension, opening a channel with ConnectInfo, adding a
// listener, writing, reading, and closing.
IN_PROC_BROWSER_TEST_F(CastChannelAPITest, MAYBE_TestOpenSendClose) {
  SetUpOpenSendClose();

  EXPECT_TRUE(RunExtensionSubtest("cast_channel/api",
                                  "test_open_send_close.html"));
}

// TODO(kmarshall): Win Dbg has a workaround that makes RunExtensionSubtest
// always return true without actually running the test. Remove when fixed.
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_TestOpenErrorSend DISABLED_TestOpenErrorSend
#else
#define MAYBE_TestOpenErrorSend TestOpenErrorSend
#endif
// Test loading extension, failing to open a channel with ConnectInfo, sending
// message on closed channel, and closing.
IN_PROC_BROWSER_TEST_F(CastChannelAPITest, MAYBE_TestOpenErrorSend) {
  SetUpOpenErrorSend();

  EXPECT_TRUE(
      RunExtensionSubtest("cast_channel/api", "test_open_error_send.html"));
}

// TODO(kmarshall): Win Dbg has a workaround that makes RunExtensionSubtest
// always return true without actually running the test. Remove when fixed.
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_TestPingTimeout DISABLED_TestPingTimeout
#else
#define MAYBE_TestPingTimeout TestPingTimeout
#endif
// Verify that timeout events are propagated through the API layer.
// (SSL, non-verified).
IN_PROC_BROWSER_TEST_F(CastChannelAPITest, MAYBE_TestPingTimeout) {
  SetUpOpenPingTimeout();

  ExtensionTestMessageListener channel_opened("channel_opened_ssl", false);
  ExtensionTestMessageListener timeout("timeout_ssl", false);
  EXPECT_TRUE(
      RunExtensionSubtest("cast_channel/api", "test_open_timeout.html"));
  EXPECT_TRUE(channel_opened.WaitUntilSatisfied());
  FireTimeout();
  EXPECT_TRUE(timeout.WaitUntilSatisfied());
}

// TODO(kmarshall): Win Dbg has a workaround that makes RunExtensionSubtest
// always return true without actually running the test. Remove when fixed.
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_TestPingTimeoutSslVerified DISABLED_TestPingTimeoutSslVerified
#else
#define MAYBE_TestPingTimeoutSslVerified TestPingTimeoutSslVerified
#endif
// Verify that timeout events are propagated through the API layer.
// (SSL, verified).
IN_PROC_BROWSER_TEST_F(CastChannelAPITest, MAYBE_TestPingTimeoutSslVerified) {
  SetUpOpenPingTimeout();

  ExtensionTestMessageListener channel_opened("channel_opened_ssl_verified",
                                              false);
  ExtensionTestMessageListener timeout("timeout_ssl_verified", false);
  EXPECT_TRUE(RunExtensionSubtest("cast_channel/api",
                                  "test_open_timeout_verified.html"));
  EXPECT_TRUE(channel_opened.WaitUntilSatisfied());
  FireTimeout();
  EXPECT_TRUE(timeout.WaitUntilSatisfied());
}

// TODO(kmarshall): Win Dbg has a workaround that makes RunExtensionSubtest
// always return true without actually running the test. Remove when fixed.
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_TestOpenReceiveClose DISABLED_TestOpenReceiveClose
#else
#define MAYBE_TestOpenReceiveClose TestOpenReceiveClose
#endif
// Test loading extension, opening a channel, adding a listener,
// writing, reading, and closing.
IN_PROC_BROWSER_TEST_F(CastChannelAPITest, MAYBE_TestOpenReceiveClose) {
  SetUpMockCastSocket();
  mock_cast_socket_->SetErrorState(ChannelError::NONE);

  {
    InSequence sequence;

    EXPECT_CALL(*mock_cast_socket_, ConnectInternal(_))
        .WillOnce(WithArgs<0>(
            Invoke([&](const MockCastSocket::MockOnOpenCallback& callback) {
              callback.Run(mock_cast_socket_);
            })));
    EXPECT_CALL(*mock_cast_socket_, ready_state())
        .WillOnce(Return(ReadyState::OPEN));
    EXPECT_CALL(*mock_cast_socket_, AddObserver(_))
        .WillOnce(SaveArg<0>(&message_observer_));
    EXPECT_CALL(*mock_cast_socket_, ready_state())
        .Times(2)
        .WillRepeatedly(Return(ReadyState::OPEN));
    EXPECT_CALL(*mock_cast_socket_, Close(_))
        .WillOnce(InvokeCompletionCallback<0>(net::OK));
    EXPECT_CALL(*mock_cast_socket_, ready_state())
        .WillOnce(Return(ReadyState::CLOSED));
  }

  EXPECT_TRUE(RunExtensionSubtest("cast_channel/api",
                                  "test_open_receive_close.html"));

  extensions::ResultCatcher catcher;
  CallOnMessage("some-message");
  CallOnMessage("some-message");
  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}

// TODO(kmarshall): Win Dbg has a workaround that makes RunExtensionSubtest
// always return true without actually running the test. Remove when fixed.
#if defined(OS_WIN) && !defined(NDEBUG)
#define MAYBE_TestOpenError DISABLED_TestOpenError
#else
#define MAYBE_TestOpenError TestOpenError
#endif
// Test the case when socket open results in an error.
IN_PROC_BROWSER_TEST_F(CastChannelAPITest, MAYBE_TestOpenError) {
  SetUpMockCastSocket();

  EXPECT_CALL(*mock_cast_socket_, AddObserver(_))
      .WillOnce(DoAll(SaveArg<0>(&message_observer_),
                      InvokeObserverOnError(this, GetCastSocketService())));
  EXPECT_CALL(*mock_cast_socket_, ConnectInternal(_))
      .WillOnce(WithArgs<0>(
          Invoke([&](const MockCastSocket::MockOnOpenCallback& callback) {
            callback.Run(mock_cast_socket_);
          })));
  mock_cast_socket_->SetErrorState(ChannelError::CONNECT_ERROR);
  EXPECT_CALL(*mock_cast_socket_, ready_state())
      .WillRepeatedly(Return(ReadyState::CLOSED));
  EXPECT_CALL(*mock_cast_socket_, Close(_))
      .WillOnce(InvokeCompletionCallback<0>(net::OK));

  EXPECT_TRUE(RunExtensionSubtest("cast_channel/api",
                                  "test_open_error.html"));
}

IN_PROC_BROWSER_TEST_F(CastChannelAPITest, TestOpenInvalidConnectInfo) {
  scoped_refptr<const Extension> empty_extension =
      extensions::ExtensionBuilder("Test").Build();
  scoped_refptr<extensions::CastChannelOpenFunction> cast_channel_open_function;

  // Invalid IP address
  cast_channel_open_function = CreateOpenFunction(empty_extension);
  std::string error = utils::RunFunctionAndReturnError(
      cast_channel_open_function.get(),
      "[{\"ipAddress\": \"invalid_ip\", \"port\": 8009, \"auth\": "
      "\"ssl_verified\"}]",
      browser());
  EXPECT_EQ(error, "Invalid connect_info (invalid IP address)");

  // Invalid port
  cast_channel_open_function = CreateOpenFunction(empty_extension);
  error = utils::RunFunctionAndReturnError(cast_channel_open_function.get(),
                                           "[{\"ipAddress\": \"127.0.0.1\", "
                                           "\"port\": -200, \"auth\": "
                                           "\"ssl_verified\"}]",
                                           browser());
  EXPECT_EQ(error, "Invalid connect_info (invalid port)");
}

IN_PROC_BROWSER_TEST_F(CastChannelAPITest, TestSendInvalidMessageInfo) {
  scoped_refptr<const Extension> empty_extension(
      extensions::ExtensionBuilder("Test").Build());
  scoped_refptr<extensions::CastChannelSendFunction> cast_channel_send_function;

  // Numbers are not supported
  cast_channel_send_function = CreateSendFunction(empty_extension);
  std::string error(utils::RunFunctionAndReturnError(
      cast_channel_send_function.get(),
      "[{\"channelId\": 1, "
      "\"keepAlive\": true, "
      "\"audioOnly\": false, "
      "\"connectInfo\": "
      "{\"ipAddress\": \"127.0.0.1\", \"port\": 8009, "
      "\"auth\": \"ssl_verified\"}, \"readyState\": \"open\"}, "
      "{\"namespace_\": \"foo\", \"sourceId\": \"src\", "
      "\"destinationId\": \"dest\", \"data\": 1235}]",
      browser()));
  EXPECT_EQ(error, "Invalid type of message_info.data");

  // Missing namespace_
  cast_channel_send_function = CreateSendFunction(empty_extension);
  error = utils::RunFunctionAndReturnError(
      cast_channel_send_function.get(),
      "[{\"channelId\": 1, "
      "\"keepAlive\": true, "
      "\"audioOnly\": false, "
      "\"connectInfo\": "
      "{\"ipAddress\": \"127.0.0.1\", \"port\": 8009, "
      "\"auth\": \"ssl_verified\"}, \"readyState\": \"open\"}, "
      "{\"namespace_\": \"\", \"sourceId\": \"src\", "
      "\"destinationId\": \"dest\", \"data\": \"data\"}]",
      browser());
  EXPECT_EQ(error, "message_info.namespace_ is required");

  // Missing source_id
  cast_channel_send_function = CreateSendFunction(empty_extension);
  error = utils::RunFunctionAndReturnError(
      cast_channel_send_function.get(),
      "[{\"channelId\": 1, "
      "\"keepAlive\": true, "
      "\"audioOnly\": false, "
      "\"connectInfo\": "
      "{\"ipAddress\": \"127.0.0.1\", \"port\": 8009, "
      "\"auth\": \"ssl_verified\"}, \"readyState\": \"open\"}, "
      "{\"namespace_\": \"foo\", \"sourceId\": \"\", "
      "\"destinationId\": \"dest\", \"data\": \"data\"}]",
      browser());
  EXPECT_EQ(error, "message_info.source_id is required");

  // Missing destination_id
  cast_channel_send_function = CreateSendFunction(empty_extension);
  error = utils::RunFunctionAndReturnError(
      cast_channel_send_function.get(),
      "[{\"channelId\": 1, "
      "\"keepAlive\": true, "
      "\"audioOnly\": false, "
      "\"connectInfo\": "
      "{\"ipAddress\": \"127.0.0.1\", \"port\": 8009, "
      "\"auth\": \"ssl_verified\"}, \"readyState\": \"open\"}, "
      "{\"namespace_\": \"foo\", \"sourceId\": \"src\", "
      "\"destinationId\": \"\", \"data\": \"data\"}]",
      browser());
  EXPECT_EQ(error, "message_info.destination_id is required");
}
