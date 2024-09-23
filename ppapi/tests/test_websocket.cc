// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ppapi/tests/test_websocket.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppb_var_array_buffer.h"
#include "ppapi/c/ppb_websocket.h"
#include "ppapi/c/private/ppb_testing_private.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var_array_buffer.h"
#include "ppapi/cpp/websocket.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"
#include "ppapi/utility/websocket/websocket_api.h"

// net::SpawnedTestServer serves WebSocket service for testing.
// Following URLs are handled by pywebsocket handlers in
// net/data/websocket/*_wsh.py.
const char kEchoServerURL[] = "echo-with-no-extension";
const char kCloseServerURL[] = "close";
const char kCloseWithCodeAndReasonServerURL[] = "close-code-and-reason";
const char kProtocolTestServerURL[] = "protocol-test?protocol=";

const char* const kInvalidURLs[] = {"http://www.google.com/invalid_scheme",
                                    "ws://www.google.com/invalid#fragment",
                                    "ws://www.google.com:7/invalid_port",
                                    NULL};

// Internal packet sizes.
const uint64_t kMessageFrameOverhead = 6;

namespace {

struct WebSocketEvent {
  enum EventType {
    EVENT_OPEN,
    EVENT_MESSAGE,
    EVENT_ERROR,
    EVENT_CLOSE
  };

  WebSocketEvent(EventType type,
                 bool was_clean,
                 uint16_t close_code,
                 const pp::Var& var)
      : event_type(type),
        was_clean(was_clean),
        close_code(close_code),
        var(var) {
  }
  EventType event_type;
  bool was_clean;
  uint16_t close_code;
  pp::Var var;
};

class ReleaseResourceDelegate : public TestCompletionCallback::Delegate {
 public:
  explicit ReleaseResourceDelegate(const PPB_Core* core_interface,
                                   PP_Resource resource)
      : core_interface_(core_interface),
        resource_(resource) {
  }

  // TestCompletionCallback::Delegate implementation.
  virtual void OnCallback(void* user_data, int32_t result) {
    if (resource_)
      core_interface_->ReleaseResource(resource_);
  }

 private:
  const PPB_Core* core_interface_;
  PP_Resource resource_;
};

class TestWebSocketAPI : public pp::WebSocketAPI {
 public:
  explicit TestWebSocketAPI(pp::Instance* instance)
      : pp::WebSocketAPI(instance),
        connected_(false),
        received_(false),
        closed_(false),
        wait_for_connected_(false),
        wait_for_received_(false),
        wait_for_closed_(false),
        instance_(instance->pp_instance()) {
  }

  virtual void WebSocketDidOpen() {
    events_.push_back(
        WebSocketEvent(WebSocketEvent::EVENT_OPEN, true, 0U, pp::Var()));
    connected_ = true;
    if (wait_for_connected_) {
      GetTestingInterface()->QuitMessageLoop(instance_);
      wait_for_connected_ = false;
    }
  }

  virtual void WebSocketDidClose(
      bool was_clean, uint16_t code, const pp::Var& reason) {
    events_.push_back(
        WebSocketEvent(WebSocketEvent::EVENT_CLOSE, was_clean, code, reason));
    connected_ = true;
    closed_ = true;
    if (wait_for_connected_ || wait_for_closed_) {
      GetTestingInterface()->QuitMessageLoop(instance_);
      wait_for_connected_ = false;
      wait_for_closed_ = false;
    }
  }

  virtual void HandleWebSocketMessage(const pp::Var &message) {
    events_.push_back(
        WebSocketEvent(WebSocketEvent::EVENT_MESSAGE, true, 0U, message));
    received_ = true;
    if (wait_for_received_) {
      GetTestingInterface()->QuitMessageLoop(instance_);
      wait_for_received_ = false;
      received_ = false;
    }
  }

  virtual void HandleWebSocketError() {
    events_.push_back(
        WebSocketEvent(WebSocketEvent::EVENT_ERROR, true, 0U, pp::Var()));
  }

  void WaitForConnected() {
    if (!connected_) {
      wait_for_connected_ = true;
      GetTestingInterface()->RunMessageLoop(instance_);
    }
  }

  void WaitForReceived() {
    if (!received_) {
      wait_for_received_ = true;
      GetTestingInterface()->RunMessageLoop(instance_);
    }
  }

  void WaitForClosed() {
    if (!closed_) {
      wait_for_closed_ = true;
      GetTestingInterface()->RunMessageLoop(instance_);
    }
  }

  const std::vector<WebSocketEvent>& GetSeenEvents() const {
    return events_;
  }

 private:
  std::vector<WebSocketEvent> events_;
  bool connected_;
  bool received_;
  bool closed_;
  bool wait_for_connected_;
  bool wait_for_received_;
  bool wait_for_closed_;
  PP_Instance instance_;
};

}  // namespace

REGISTER_TEST_CASE(WebSocket);

bool TestWebSocket::Init() {
  websocket_interface_ = static_cast<const PPB_WebSocket*>(
      pp::Module::Get()->GetBrowserInterface(PPB_WEBSOCKET_INTERFACE));
  var_interface_ = static_cast<const PPB_Var*>(
      pp::Module::Get()->GetBrowserInterface(PPB_VAR_INTERFACE));
  arraybuffer_interface_ = static_cast<const PPB_VarArrayBuffer*>(
      pp::Module::Get()->GetBrowserInterface(
          PPB_VAR_ARRAY_BUFFER_INTERFACE));
  core_interface_ = static_cast<const PPB_Core*>(
      pp::Module::Get()->GetBrowserInterface(PPB_CORE_INTERFACE));
  if (!websocket_interface_ || !var_interface_ || !arraybuffer_interface_ ||
      !core_interface_)
    return false;

  return CheckTestingInterface();
}

void TestWebSocket::RunTests(const std::string& filter) {
  RUN_TEST_WITH_REFERENCE_CHECK(IsWebSocket, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(UninitializedPropertiesAccess, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(InvalidConnect, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(Protocols, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(GetURL, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(ValidConnect, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(InvalidClose, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(ValidClose, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(GetProtocol, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(TextSendReceive, filter);
  RUN_TEST_BACKGROUND(TestWebSocket, TextSendReceiveTwice, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(BinarySendReceive, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(StressedSendReceive, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(BufferedAmount, filter);
  // PP_Resource for WebSocket may be released later because of an internal
  // reference for asynchronous IPC handling. So, suppress reference check on
  // the following AbortCallsWithCallback test.
  RUN_TEST(AbortCallsWithCallback, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(AbortSendMessageCall, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(AbortCloseCall, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(AbortReceiveMessageCall, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(ClosedFromServerWhileSending, filter);

  RUN_TEST_WITH_REFERENCE_CHECK(CcInterfaces, filter);

  RUN_TEST_WITH_REFERENCE_CHECK(UtilityInvalidConnect, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(UtilityProtocols, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(UtilityGetURL, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(UtilityValidConnect, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(UtilityInvalidClose, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(UtilityValidClose, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(UtilityGetProtocol, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(UtilityTextSendReceive, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(UtilityBinarySendReceive, filter);
  RUN_TEST_WITH_REFERENCE_CHECK(UtilityBufferedAmount, filter);
}

std::string TestWebSocket::GetFullURL(const char* url) {
  std::string rv = "ws://";
  // Some WebSocket tests don't start the server so there'll be no host and
  // port.
  if (instance_->websocket_host().empty())
    rv += "127.0.0.1";
  else
    rv += instance_->websocket_host();
  if (instance_->websocket_port() != -1) {
    char buffer[10];
    snprintf(buffer, sizeof(buffer), ":%d", instance_->websocket_port());
    rv += std::string(buffer);
  }
  rv += "/";
  rv += url;
  return rv;
}

PP_Var TestWebSocket::CreateVarString(const std::string& string) {
  return var_interface_->VarFromUtf8(string.c_str(),
                                     static_cast<uint32_t>(string.size()));
}

PP_Var TestWebSocket::CreateVarBinary(const std::vector<uint8_t>& binary) {
  PP_Var var =
      arraybuffer_interface_->Create(static_cast<uint32_t>(binary.size()));
  uint8_t* var_data = static_cast<uint8_t*>(arraybuffer_interface_->Map(var));
  std::copy(binary.begin(), binary.end(), var_data);
  return var;
}

void TestWebSocket::ReleaseVar(const PP_Var& var) {
  var_interface_->Release(var);
}

bool TestWebSocket::AreEqualWithString(const PP_Var& var,
                                       const std::string& string) {
  if (var.type != PP_VARTYPE_STRING)
    return false;
  uint32_t utf8_length;
  const char* utf8 = var_interface_->VarToUtf8(var, &utf8_length);
  if (utf8_length != string.size())
    return false;
  if (string.compare(utf8))
    return false;
  return true;
}

bool TestWebSocket::AreEqualWithBinary(const PP_Var& var,
                                       const std::vector<uint8_t>& binary) {
  uint32_t buffer_size = 0;
  PP_Bool success = arraybuffer_interface_->ByteLength(var, &buffer_size);
  if (!success || buffer_size != binary.size())
    return false;
  if (!std::equal(binary.begin(), binary.end(),
      static_cast<uint8_t*>(arraybuffer_interface_->Map(var))))
    return false;
  return true;
}

PP_Resource TestWebSocket::Connect(const std::string& url,
                                   int32_t* result,
                                   const std::string& protocol) {
  PP_Var protocols[] = { PP_MakeUndefined() };
  PP_Resource ws = websocket_interface_->Create(instance_->pp_instance());
  if (!ws)
    return 0;
  PP_Var url_var = CreateVarString(url);
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  uint32_t protocol_count = 0U;
  if (protocol.size()) {
    protocols[0] = CreateVarString(protocol);
    protocol_count = 1U;
  }
  callback.WaitForResult(websocket_interface_->Connect(
      ws, url_var, protocols, protocol_count,
      callback.GetCallback().pp_completion_callback()));
  ReleaseVar(url_var);
  if (protocol.size())
    ReleaseVar(protocols[0]);
  *result = callback.result();
  return ws;
}

void TestWebSocket::Send(int32_t /* result */, PP_Resource ws,
                         const std::string& message) {
  PP_Var message_var = CreateVarString(message);
  websocket_interface_->SendMessage(ws, message_var);
  ReleaseVar(message_var);
}

std::string TestWebSocket::TestIsWebSocket() {
  // Test that a NULL resource isn't a websocket.
  pp::Resource null_resource;
  PP_Bool result =
      websocket_interface_->IsWebSocket(null_resource.pp_resource());
  ASSERT_FALSE(result);

  PP_Resource ws = websocket_interface_->Create(instance_->pp_instance());
  ASSERT_TRUE(ws);

  result = websocket_interface_->IsWebSocket(ws);
  ASSERT_TRUE(result);

  core_interface_->ReleaseResource(ws);

  PASS();
}

std::string TestWebSocket::TestUninitializedPropertiesAccess() {
  PP_Resource ws = websocket_interface_->Create(instance_->pp_instance());
  ASSERT_TRUE(ws);

  uint64_t bufferedAmount = websocket_interface_->GetBufferedAmount(ws);
  ASSERT_EQ(0U, bufferedAmount);

  uint16_t close_code = websocket_interface_->GetCloseCode(ws);
  ASSERT_EQ(0U, close_code);

  PP_Var close_reason = websocket_interface_->GetCloseReason(ws);
  ASSERT_TRUE(AreEqualWithString(close_reason, std::string()));
  ReleaseVar(close_reason);

  PP_Bool close_was_clean = websocket_interface_->GetCloseWasClean(ws);
  ASSERT_EQ(PP_FALSE, close_was_clean);

  PP_Var extensions = websocket_interface_->GetExtensions(ws);
  ASSERT_TRUE(AreEqualWithString(extensions, std::string()));
  ReleaseVar(extensions);

  PP_Var protocol = websocket_interface_->GetProtocol(ws);
  ASSERT_TRUE(AreEqualWithString(protocol, std::string()));
  ReleaseVar(protocol);

  PP_WebSocketReadyState ready_state =
      websocket_interface_->GetReadyState(ws);
  ASSERT_EQ(PP_WEBSOCKETREADYSTATE_INVALID, ready_state);

  PP_Var url = websocket_interface_->GetURL(ws);
  ASSERT_TRUE(AreEqualWithString(url, std::string()));
  ReleaseVar(url);

  core_interface_->ReleaseResource(ws);

  PASS();
}

std::string TestWebSocket::TestInvalidConnect() {
  PP_Var protocols[] = { PP_MakeUndefined() };

  PP_Resource ws = websocket_interface_->Create(instance_->pp_instance());
  ASSERT_TRUE(ws);

  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  callback.WaitForResult(websocket_interface_->Connect(
      ws, PP_MakeUndefined(), protocols, 1U,
      callback.GetCallback().pp_completion_callback()));
  ASSERT_EQ(PP_ERROR_BADARGUMENT, callback.result());

  callback.WaitForResult(websocket_interface_->Connect(
      ws, PP_MakeUndefined(), protocols, 1U,
      callback.GetCallback().pp_completion_callback()));
  ASSERT_EQ(PP_ERROR_INPROGRESS, callback.result());

  core_interface_->ReleaseResource(ws);

  for (int i = 0; kInvalidURLs[i]; ++i) {
    int32_t result;
    ws = Connect(kInvalidURLs[i], &result, std::string());
    ASSERT_TRUE(ws);
    ASSERT_EQ(PP_ERROR_BADARGUMENT, result);

    core_interface_->ReleaseResource(ws);
  }

  PASS();
}

std::string TestWebSocket::TestProtocols() {
  PP_Var url = CreateVarString(GetFullURL(kEchoServerURL).c_str());
  PP_Var bad_protocols[] = {
    CreateVarString("x-test"),
    CreateVarString("x-test")
  };
  PP_Var good_protocols[] = {
    CreateVarString("x-test"),
    CreateVarString("x-yatest")
  };

  PP_Resource ws = websocket_interface_->Create(instance_->pp_instance());
  ASSERT_TRUE(ws);
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  callback.WaitForResult(websocket_interface_->Connect(
      ws, url, bad_protocols, 2U,
      callback.GetCallback().pp_completion_callback()));
  ASSERT_EQ(PP_ERROR_BADARGUMENT, callback.result());
  core_interface_->ReleaseResource(ws);

  ws = websocket_interface_->Create(instance_->pp_instance());
  ASSERT_TRUE(ws);
  int32_t result = websocket_interface_->Connect(
      ws, url, good_protocols, 2U, PP_BlockUntilComplete());
  ASSERT_EQ(PP_ERROR_BLOCKS_MAIN_THREAD, result);
  core_interface_->ReleaseResource(ws);

  ReleaseVar(url);
  for (int i = 0; i < 2; ++i) {
    ReleaseVar(bad_protocols[i]);
    ReleaseVar(good_protocols[i]);
  }
  core_interface_->ReleaseResource(ws);

  PASS();
}

std::string TestWebSocket::TestGetURL() {
  for (int i = 0; kInvalidURLs[i]; ++i) {
    int32_t result;
    PP_Resource ws = Connect(kInvalidURLs[i], &result, std::string());
    ASSERT_TRUE(ws);
    PP_Var url = websocket_interface_->GetURL(ws);
    ASSERT_TRUE(AreEqualWithString(url, kInvalidURLs[i]));
    ASSERT_EQ(PP_ERROR_BADARGUMENT, result);

    ReleaseVar(url);
    core_interface_->ReleaseResource(ws);
  }

  PASS();
}

std::string TestWebSocket::TestValidConnect() {
  int32_t result;
  PP_Resource ws = Connect(GetFullURL(kEchoServerURL), &result, std::string());
  ASSERT_TRUE(ws);
  ASSERT_EQ(PP_OK, result);
  PP_Var extensions = websocket_interface_->GetExtensions(ws);
  ASSERT_TRUE(AreEqualWithString(extensions, std::string()));
  core_interface_->ReleaseResource(ws);
  ReleaseVar(extensions);

  PASS();
}

std::string TestWebSocket::TestInvalidClose() {
  PP_Var reason = CreateVarString("close for test");
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  TestCompletionCallback async_callback(instance_->pp_instance(), PP_REQUIRED);

  // Close before connect.
  PP_Resource ws = websocket_interface_->Create(instance_->pp_instance());
  callback.WaitForResult(websocket_interface_->Close(
      ws, PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, reason,
      callback.GetCallback().pp_completion_callback()));
  ASSERT_EQ(PP_ERROR_FAILED, callback.result());
  core_interface_->ReleaseResource(ws);

  // Close with bad arguments.
  int32_t result;
  ws = Connect(GetFullURL(kEchoServerURL), &result, std::string());
  ASSERT_TRUE(ws);
  ASSERT_EQ(PP_OK, result);
  callback.WaitForResult(websocket_interface_->Close(
      ws, 1U, reason, callback.GetCallback().pp_completion_callback()));
  ASSERT_EQ(PP_ERROR_NOACCESS, callback.result());
  core_interface_->ReleaseResource(ws);

  // Close with PP_VARTYPE_NULL.
  ws = Connect(GetFullURL(kEchoServerURL), &result, std::string());
  ASSERT_TRUE(ws);
  ASSERT_EQ(PP_OK, result);
  callback.WaitForResult(websocket_interface_->Close(
      ws, PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, PP_MakeNull(),
      callback.GetCallback().pp_completion_callback()));
  ASSERT_EQ(PP_ERROR_BADARGUMENT, callback.result());
  core_interface_->ReleaseResource(ws);

  // Close with PP_VARTYPE_NULL and ongoing receive message.
  ws = Connect(GetFullURL(kEchoServerURL), &result, std::string());
  ASSERT_TRUE(ws);
  ASSERT_EQ(PP_OK, result);
  PP_Var receive_message_var;
  result = websocket_interface_->ReceiveMessage(
      ws, &receive_message_var,
      async_callback.GetCallback().pp_completion_callback());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  callback.WaitForResult(websocket_interface_->Close(
      ws, PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, PP_MakeNull(),
      callback.GetCallback().pp_completion_callback()));
  ASSERT_EQ(PP_ERROR_BADARGUMENT, callback.result());
  const char* send_message = "hi";
  PP_Var send_message_var = CreateVarString(send_message);
  result = websocket_interface_->SendMessage(ws, send_message_var);
  ReleaseVar(send_message_var);
  ASSERT_EQ(PP_OK, result);
  async_callback.WaitForResult(PP_OK_COMPLETIONPENDING);
  ASSERT_EQ(PP_OK, async_callback.result());
  ASSERT_TRUE(AreEqualWithString(receive_message_var, send_message));
  ReleaseVar(receive_message_var);
  core_interface_->ReleaseResource(ws);

  // Close twice.
  ws = Connect(GetFullURL(kEchoServerURL), &result, std::string());
  ASSERT_TRUE(ws);
  ASSERT_EQ(PP_OK, result);
  result = websocket_interface_->Close(
      ws, PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, reason,
      async_callback.GetCallback().pp_completion_callback());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  // Call another Close() before previous one is in progress.
  result = websocket_interface_->Close(
      ws, PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, reason,
      callback.GetCallback().pp_completion_callback());
  ASSERT_EQ(PP_ERROR_INPROGRESS, result);
  async_callback.WaitForResult(PP_OK_COMPLETIONPENDING);
  ASSERT_EQ(PP_OK, async_callback.result());
  // Call another Close() after previous one is completed.
  // This Close() must do nothing and reports no error.
  callback.WaitForResult(websocket_interface_->Close(
      ws, PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, reason,
      callback.GetCallback().pp_completion_callback()));
  ASSERT_EQ(PP_OK, callback.result());
  core_interface_->ReleaseResource(ws);

  ReleaseVar(reason);

  PASS();
}

// TODO(tyoshino): Consider splitting this test into smaller ones.
// http://crbug.com/397035
std::string TestWebSocket::TestValidClose() {
  PP_Var reason = CreateVarString("close for test");
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  TestCompletionCallback another_callback(
      instance_->pp_instance(), callback_type());

  // Close.
  int32_t result;
  PP_Resource ws = Connect(GetFullURL(kEchoServerURL), &result, std::string());
  ASSERT_TRUE(ws);
  ASSERT_EQ(PP_OK, result);
  callback.WaitForResult(websocket_interface_->Close(
      ws, PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, reason,
      callback.GetCallback().pp_completion_callback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());
  core_interface_->ReleaseResource(ws);

  // Close without code and reason.
  ws = Connect(GetFullURL(kEchoServerURL), &result, std::string());
  ASSERT_TRUE(ws);
  ASSERT_EQ(PP_OK, result);
  callback.WaitForResult(websocket_interface_->Close(
      ws, PP_WEBSOCKETSTATUSCODE_NOT_SPECIFIED, reason,
      callback.GetCallback().pp_completion_callback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());
  core_interface_->ReleaseResource(ws);

  // Close with PP_VARTYPE_UNDEFINED.
  ws = Connect(GetFullURL(kEchoServerURL), &result, std::string());
  ASSERT_TRUE(ws);
  ASSERT_EQ(PP_OK, result);
  callback.WaitForResult(websocket_interface_->Close(
      ws, PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, PP_MakeUndefined(),
      callback.GetCallback().pp_completion_callback()));
  CHECK_CALLBACK_BEHAVIOR(callback);
  ASSERT_EQ(PP_OK, callback.result());
  core_interface_->ReleaseResource(ws);

  // Close in CONNECTING state.
  // The ongoing Connect() fails with PP_ERROR_ABORTED, then the Close()
  // completes successfully.
  ws = websocket_interface_->Create(instance_->pp_instance());
  PP_Var url = CreateVarString(GetFullURL(kEchoServerURL).c_str());
  PP_Var protocols[] = { PP_MakeUndefined() };
  result = websocket_interface_->Connect(
      ws, url, protocols, 0U, callback.GetCallback().pp_completion_callback());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  result = websocket_interface_->Close(
      ws, PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, reason,
      another_callback.GetCallback().pp_completion_callback());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  callback.WaitForResult(PP_OK_COMPLETIONPENDING);
  ASSERT_EQ(PP_ERROR_ABORTED, callback.result());
  another_callback.WaitForResult(PP_OK_COMPLETIONPENDING);
  ASSERT_EQ(PP_OK, another_callback.result());
  core_interface_->ReleaseResource(ws);
  ReleaseVar(url);

  // Close while already closing.
  // The first Close will succeed, and the second one will synchronously fail
  // with PP_ERROR_INPROGRESS.
  ws = Connect(GetFullURL(kEchoServerURL), &result, std::string());
  ASSERT_TRUE(ws);
  ASSERT_EQ(PP_OK, result);
  result = websocket_interface_->Close(
      ws, PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, reason,
      callback.GetCallback().pp_completion_callback());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  result = websocket_interface_->Close(
      ws, PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, reason,
      another_callback.GetCallback().pp_completion_callback());
  ASSERT_EQ(PP_ERROR_INPROGRESS, result);
  callback.WaitForResult(PP_OK_COMPLETIONPENDING);
  ASSERT_EQ(PP_OK, callback.result());
  core_interface_->ReleaseResource(ws);

  // Close with ongoing ReceiveMessage.
  ws = Connect(GetFullURL(kEchoServerURL), &result, std::string());
  ASSERT_TRUE(ws);
  ASSERT_EQ(PP_OK, result);
  PP_Var receive_message_var;
  result = websocket_interface_->ReceiveMessage(
      ws, &receive_message_var,
      callback.GetCallback().pp_completion_callback());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  result = websocket_interface_->Close(
      ws, PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, reason,
      another_callback.GetCallback().pp_completion_callback());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  callback.WaitForResult(PP_OK_COMPLETIONPENDING);
  ASSERT_EQ(PP_ERROR_ABORTED, callback.result());
  another_callback.WaitForResult(PP_OK_COMPLETIONPENDING);
  ASSERT_EQ(PP_OK, another_callback.result());
  core_interface_->ReleaseResource(ws);

  // Close with PP_VARTYPE_UNDEFINED for reason and ongoing ReceiveMessage.
  ws = Connect(GetFullURL(kEchoServerURL), &result, std::string());
  ASSERT_TRUE(ws);
  ASSERT_EQ(PP_OK, result);
  result = websocket_interface_->ReceiveMessage(
      ws, &receive_message_var,
      callback.GetCallback().pp_completion_callback());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  result = websocket_interface_->Close(
      ws, PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, PP_MakeUndefined(),
      another_callback.GetCallback().pp_completion_callback());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  callback.WaitForResult(PP_OK_COMPLETIONPENDING);
  ASSERT_EQ(PP_ERROR_ABORTED, callback.result());
  another_callback.WaitForResult(PP_OK_COMPLETIONPENDING);
  ASSERT_EQ(PP_OK, another_callback.result());
  core_interface_->ReleaseResource(ws);

  // Server initiated closing handshake.
  ws = Connect(
      GetFullURL(kCloseWithCodeAndReasonServerURL), &result, std::string());
  ASSERT_TRUE(ws);
  ASSERT_EQ(PP_OK, result);
  // Text messsage "1000 bye" requests the server to initiate closing handshake
  // with code being 1000 and reason being "bye".
  PP_Var close_request_var = CreateVarString("1000 bye");
  result = websocket_interface_->SendMessage(ws, close_request_var);
  ReleaseVar(close_request_var);
  callback.WaitForResult(websocket_interface_->ReceiveMessage(
      ws, &receive_message_var,
      callback.GetCallback().pp_completion_callback()));
  ASSERT_EQ(PP_ERROR_FAILED, callback.result());
  core_interface_->ReleaseResource(ws);

  ReleaseVar(reason);

  PASS();
}

std::string TestWebSocket::TestGetProtocol() {
  const char* expected_protocols[] = {
    "x-chat",
    "hoehoe",
    NULL
  };
  for (int i = 0; expected_protocols[i]; ++i) {
    std::string url(GetFullURL(kProtocolTestServerURL));
    url += expected_protocols[i];
    int32_t result;
    PP_Resource ws = Connect(url.c_str(), &result, expected_protocols[i]);
    ASSERT_TRUE(ws);
    ASSERT_EQ(PP_OK, result);

    PP_Var protocol = websocket_interface_->GetProtocol(ws);
    ASSERT_TRUE(AreEqualWithString(protocol, expected_protocols[i]));

    ReleaseVar(protocol);
    core_interface_->ReleaseResource(ws);
  }

  PASS();
}

std::string TestWebSocket::TestTextSendReceive() {
  // Connect to test echo server.
  int32_t connect_result;
  PP_Resource ws =
      Connect(GetFullURL(kEchoServerURL), &connect_result, std::string());
  ASSERT_TRUE(ws);
  ASSERT_EQ(PP_OK, connect_result);

  // Send 'hello pepper' text message.
  const char* message = "hello pepper";
  PP_Var message_var = CreateVarString(message);
  int32_t result = websocket_interface_->SendMessage(ws, message_var);
  ReleaseVar(message_var);
  ASSERT_EQ(PP_OK, result);

  // Receive echoed 'hello pepper'.
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  PP_Var received_message;
  callback.WaitForResult(websocket_interface_->ReceiveMessage(
      ws, &received_message, callback.GetCallback().pp_completion_callback()));
  ASSERT_EQ(PP_OK, callback.result());
  ASSERT_TRUE(AreEqualWithString(received_message, message));
  ReleaseVar(received_message);
  core_interface_->ReleaseResource(ws);

  PASS();
}

// Run as a BACKGROUND test.
std::string TestWebSocket::TestTextSendReceiveTwice() {
  // Connect to test echo server.
  int32_t connect_result;
  PP_Resource ws =
      Connect(GetFullURL(kEchoServerURL), &connect_result, std::string());
  ASSERT_TRUE(ws);
  ASSERT_EQ(PP_OK, connect_result);
  pp::MessageLoop message_loop = pp::MessageLoop::GetCurrent();
  pp::CompletionCallbackFactory<TestWebSocket> factory(this);

  message_loop.PostWork(factory.NewCallback(&TestWebSocket::Send,
                                            ws, std::string("hello")));
  // When the server receives 'Goodbye', it closes the session.
  message_loop.PostWork(factory.NewCallback(&TestWebSocket::Send,
                                            ws, std::string("Goodbye")));
  message_loop.PostQuit(false);
  message_loop.Run();

  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  PP_Var received_message;
  int32_t result = websocket_interface_->ReceiveMessage(
      ws, &received_message, callback.GetCallback().pp_completion_callback());
  ASSERT_EQ(PP_OK, result);
  // Since we don't run the message loop, the callback will stay
  // "pending and scheduled to run" state.

  // Waiting for the connection close which will be done by the server.
  while (true) {
    PP_WebSocketReadyState ready_state =
        websocket_interface_->GetReadyState(ws);
    if (ready_state != PP_WEBSOCKETREADYSTATE_CONNECTING &&
        ready_state != PP_WEBSOCKETREADYSTATE_OPEN) {
      break;
    }
    PlatformSleep(100);  // 100ms
  }

  // Cleanup the message loop
  message_loop.PostQuit(false);
  message_loop.Run();

  ASSERT_EQ(PP_OK, callback.result());
  ASSERT_TRUE(AreEqualWithString(received_message, "hello"));
  ReleaseVar(received_message);
  core_interface_->ReleaseResource(ws);
  PASS();
}

std::string TestWebSocket::TestBinarySendReceive() {
  // Connect to test echo server.
  int32_t connect_result;
  PP_Resource ws =
      Connect(GetFullURL(kEchoServerURL), &connect_result, std::string());
  ASSERT_TRUE(ws);
  ASSERT_EQ(PP_OK, connect_result);

  // Send binary message.
  std::vector<uint8_t> binary(256);
  for (uint32_t i = 0; i < binary.size(); ++i)
    binary[i] = i;
  PP_Var message_var = CreateVarBinary(binary);
  int32_t result = websocket_interface_->SendMessage(ws, message_var);
  ReleaseVar(message_var);
  ASSERT_EQ(PP_OK, result);

  // Receive echoed binary.
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  PP_Var received_message;
  callback.WaitForResult(websocket_interface_->ReceiveMessage(
      ws, &received_message, callback.GetCallback().pp_completion_callback()));
  ASSERT_EQ(PP_OK, callback.result());
  ASSERT_TRUE(AreEqualWithBinary(received_message, binary));
  ReleaseVar(received_message);
  core_interface_->ReleaseResource(ws);

  PASS();
}

std::string TestWebSocket::TestStressedSendReceive() {
  // Connect to test echo server.
  int32_t connect_result;
  PP_Resource ws =
      Connect(GetFullURL(kEchoServerURL), &connect_result, std::string());
  ASSERT_TRUE(ws);
  ASSERT_EQ(PP_OK, connect_result);

  // Prepare PP_Var objects to send.
  const char* text = "hello pepper";
  PP_Var text_var = CreateVarString(text);
  std::vector<uint8_t> binary(256);
  for (uint32_t i = 0; i < binary.size(); ++i)
    binary[i] = i;
  PP_Var binary_var = CreateVarBinary(binary);
  // Prepare very large binary data over 64KiB. Object serializer in
  // ppapi_proxy has a limitation of 64KiB as maximum return PP_Var data size
  // to SRPC. In case received data over 64KiB exists, a specific code handles
  // this large data via asynchronous callback from main thread. This data
  // intends to test the code.
  std::vector<uint8_t> large_binary(65 * 1024);
  for (uint32_t i = 0; i < large_binary.size(); ++i)
    large_binary[i] = i & 0xff;
  PP_Var large_binary_var = CreateVarBinary(large_binary);

  // Send many messages.
  int32_t result;
  for (int i = 0; i < 256; ++i) {
    result = websocket_interface_->SendMessage(ws, text_var);
    ASSERT_EQ(PP_OK, result);
    result = websocket_interface_->SendMessage(ws, binary_var);
    ASSERT_EQ(PP_OK, result);
  }
  result = websocket_interface_->SendMessage(ws, large_binary_var);
  ASSERT_EQ(PP_OK, result);
  ReleaseVar(text_var);
  ReleaseVar(binary_var);
  ReleaseVar(large_binary_var);

  // Receive echoed data.
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  for (int i = 0; i <= 512; ++i) {
    PP_Var received_message;
    callback.WaitForResult(websocket_interface_->ReceiveMessage(
        ws, &received_message,
        callback.GetCallback().pp_completion_callback()));
    ASSERT_EQ(PP_OK, callback.result());
    if (i == 512) {
      ASSERT_TRUE(AreEqualWithBinary(received_message, large_binary));
    } else if (i & 1) {
      ASSERT_TRUE(AreEqualWithBinary(received_message, binary));
    } else {
      ASSERT_TRUE(AreEqualWithString(received_message, text));
    }
    ReleaseVar(received_message);
  }
  core_interface_->ReleaseResource(ws);

  PASS();
}

std::string TestWebSocket::TestBufferedAmount() {
  // Connect to test echo server.
  int32_t connect_result;
  PP_Resource ws =
      Connect(GetFullURL(kEchoServerURL), &connect_result, std::string());
  ASSERT_TRUE(ws);
  ASSERT_EQ(PP_OK, connect_result);

  // Prepare a large message that is not aligned with the internal buffer
  // sizes.
  std::string message(8193, 'x');
  PP_Var message_var = CreateVarString(message);

  uint64_t buffered_amount = 0;
  int32_t result;
  for (int i = 0; i < 100; i++) {
    result = websocket_interface_->SendMessage(ws, message_var);
    ASSERT_EQ(PP_OK, result);
    buffered_amount = websocket_interface_->GetBufferedAmount(ws);
    // Buffered amount size 262144 is too big for the internal buffer size.
    if (buffered_amount > 262144)
      break;
  }

  // Close connection.
  std::string reason_str = "close while busy";
  PP_Var reason = CreateVarString(reason_str.c_str());
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  result = websocket_interface_->Close(
      ws, PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, reason,
      callback.GetCallback().pp_completion_callback());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  ASSERT_EQ(PP_WEBSOCKETREADYSTATE_CLOSING,
      websocket_interface_->GetReadyState(ws));

  callback.WaitForResult(result);
  ASSERT_EQ(PP_OK, callback.result());
  ASSERT_EQ(PP_WEBSOCKETREADYSTATE_CLOSED,
      websocket_interface_->GetReadyState(ws));

  uint64_t base_buffered_amount = websocket_interface_->GetBufferedAmount(ws);

  // After connection closure, all sending requests fail and just increase
  // the bufferedAmount property.
  PP_Var empty_string = CreateVarString(std::string());
  result = websocket_interface_->SendMessage(ws, empty_string);
  ASSERT_EQ(PP_ERROR_FAILED, result);
  buffered_amount = websocket_interface_->GetBufferedAmount(ws);
  ASSERT_EQ(base_buffered_amount + kMessageFrameOverhead, buffered_amount);
  base_buffered_amount = buffered_amount;

  result = websocket_interface_->SendMessage(ws, reason);
  ASSERT_EQ(PP_ERROR_FAILED, result);
  buffered_amount = websocket_interface_->GetBufferedAmount(ws);
  uint64_t reason_frame_size = kMessageFrameOverhead + reason_str.length();
  ASSERT_EQ(base_buffered_amount + reason_frame_size, buffered_amount);

  ReleaseVar(message_var);
  ReleaseVar(reason);
  ReleaseVar(empty_string);
  core_interface_->ReleaseResource(ws);

  PASS();
}

// Test abort behaviors where a WebSocket PP_Resource is released while each
// function is in-flight on the WebSocket PP_Resource.
std::string TestWebSocket::TestAbortCallsWithCallback() {
  // Following tests make sure the behavior for functions which require a
  // callback. The callback must get a PP_ERROR_ABORTED.

  // Test the behavior for Connect().
  PP_Resource ws = websocket_interface_->Create(instance_->pp_instance());
  ASSERT_TRUE(ws);
  std::string url = GetFullURL(kEchoServerURL);
  PP_Var url_var = CreateVarString(url);
  TestCompletionCallback connect_callback(
      instance_->pp_instance(), callback_type());
  int32_t result = websocket_interface_->Connect(
      ws, url_var, NULL, 0,
      connect_callback.GetCallback().pp_completion_callback());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  core_interface_->ReleaseResource(ws);
  connect_callback.WaitForResult(result);
  ASSERT_EQ(PP_ERROR_ABORTED, connect_callback.result());

  // Test the behavior for Close().
  ws = Connect(url, &result, std::string());
  ASSERT_TRUE(ws);
  ASSERT_EQ(PP_OK, result);
  PP_Var reason_var = CreateVarString("abort");
  TestCompletionCallback close_callback(
      instance_->pp_instance(), callback_type());
  result = websocket_interface_->Close(
      ws, PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, reason_var,
      close_callback.GetCallback().pp_completion_callback());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  core_interface_->ReleaseResource(ws);
  close_callback.WaitForResult(result);
  ASSERT_EQ(PP_ERROR_ABORTED, close_callback.result());
  ReleaseVar(reason_var);

  // Test the behavior for ReceiveMessage().
  // Make sure the simplest case to wait for data which never arrives, here.
  ws = Connect(url, &result, std::string());
  ASSERT_TRUE(ws);
  ASSERT_EQ(PP_OK, result);
  PP_Var receive_var;
  TestCompletionCallback receive_callback(
      instance_->pp_instance(), callback_type());
  result = websocket_interface_->ReceiveMessage(
      ws, &receive_var,
      receive_callback.GetCallback().pp_completion_callback());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  core_interface_->ReleaseResource(ws);
  receive_callback.WaitForResult(result);
  ASSERT_EQ(PP_ERROR_ABORTED, receive_callback.result());

  // Release the resource in the aborting receive completion callback which is
  // introduced by calling Close().
  ws = Connect(url, &result, std::string());
  ASSERT_TRUE(ws);
  ASSERT_EQ(PP_OK, result);
  result = websocket_interface_->ReceiveMessage(
      ws, &receive_var,
      receive_callback.GetCallback().pp_completion_callback());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  ReleaseResourceDelegate receive_delegate(core_interface_, ws);
  receive_callback.SetDelegate(&receive_delegate);
  result = websocket_interface_->Close(
      ws, PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, PP_MakeUndefined(),
      close_callback.GetCallback().pp_completion_callback());
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  receive_callback.WaitForResult(result);
  CHECK_CALLBACK_BEHAVIOR(receive_callback);
  ASSERT_EQ(PP_ERROR_ABORTED, receive_callback.result());
  close_callback.WaitForResult(result);
  CHECK_CALLBACK_BEHAVIOR(close_callback);
  ASSERT_EQ(PP_ERROR_ABORTED, close_callback.result());

  ReleaseVar(url_var);

  PASS();
}

std::string TestWebSocket::TestAbortSendMessageCall() {
  // Test the behavior for SendMessage().
  // This function doesn't require a callback, but operation will be done
  // asynchronously in WebKit and browser process.
  std::vector<uint8_t> large_binary(65 * 1024);
  PP_Var large_var = CreateVarBinary(large_binary);

  int32_t result;
  std::string url = GetFullURL(kEchoServerURL);
  PP_Resource ws = Connect(url, &result, std::string());
  ASSERT_TRUE(ws);
  ASSERT_EQ(PP_OK, result);
  result = websocket_interface_->SendMessage(ws, large_var);
  ASSERT_EQ(PP_OK, result);
  core_interface_->ReleaseResource(ws);
  ReleaseVar(large_var);

  PASS();
}

std::string TestWebSocket::TestAbortCloseCall() {
  // Release the resource in the close completion callback.
  int32_t result;
  std::string url = GetFullURL(kEchoServerURL);
  PP_Resource ws = Connect(url, &result, std::string());
  ASSERT_TRUE(ws);
  ASSERT_EQ(PP_OK, result);
  TestCompletionCallback close_callback(
      instance_->pp_instance(), callback_type());
  result = websocket_interface_->Close(
      ws, PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, PP_MakeUndefined(),
      close_callback.GetCallback().pp_completion_callback());
  ReleaseResourceDelegate close_delegate(core_interface_, ws);
  close_callback.SetDelegate(&close_delegate);
  close_callback.WaitForResult(result);
  CHECK_CALLBACK_BEHAVIOR(close_callback);
  ASSERT_EQ(PP_OK, close_callback.result());

  PASS();
}

std::string TestWebSocket::TestAbortReceiveMessageCall() {
  // Test the behavior where receive process might be in-flight.
  std::vector<uint8_t> large_binary(65 * 1024);
  PP_Var large_var = CreateVarBinary(large_binary);
  const char* text = "yukarin";
  PP_Var text_var = CreateVarString(text);

  std::string url = GetFullURL(kEchoServerURL);
  int32_t result;
  PP_Resource ws;

  // Each trial sends |trial_count| + 1 messages and receives just |trial|
  // number of message(s) before releasing the WebSocket. The WebSocket is
  // released while the next message is going to be received.
  const int trial_count = 8;
  for (int trial = 1; trial <= trial_count; trial++) {
    ws = Connect(url, &result, std::string());
    ASSERT_TRUE(ws);
    ASSERT_EQ(PP_OK, result);
    for (int i = 0; i <= trial_count; ++i) {
      result = websocket_interface_->SendMessage(ws, text_var);
      ASSERT_EQ(PP_OK, result);
    }
    TestCompletionCallback callback(instance_->pp_instance(), callback_type());
    PP_Var var;
    for (int i = 0; i < trial; ++i) {
      callback.WaitForResult(websocket_interface_->ReceiveMessage(
          ws, &var, callback.GetCallback().pp_completion_callback()));
      ASSERT_EQ(PP_OK, callback.result());
      ASSERT_TRUE(AreEqualWithString(var, text));
      ReleaseVar(var);
    }
    result = websocket_interface_->ReceiveMessage(
        ws, &var, callback.GetCallback().pp_completion_callback());
    core_interface_->ReleaseResource(ws);
    if (result != PP_OK) {
      callback.WaitForResult(result);
      ASSERT_EQ(PP_ERROR_ABORTED, callback.result());
    }
  }
  // Same test, but the last receiving message is large message over 64KiB.
  for (int trial = 1; trial <= trial_count; trial++) {
    ws = Connect(url, &result, std::string());
    ASSERT_TRUE(ws);
    ASSERT_EQ(PP_OK, result);
    for (int i = 0; i <= trial_count; ++i) {
      if (i == trial)
        result = websocket_interface_->SendMessage(ws, large_var);
      else
        result = websocket_interface_->SendMessage(ws, text_var);
      ASSERT_EQ(PP_OK, result);
    }
    TestCompletionCallback callback(instance_->pp_instance(), callback_type());
    PP_Var var;
    for (int i = 0; i < trial; ++i) {
      callback.WaitForResult(websocket_interface_->ReceiveMessage(
          ws, &var, callback.GetCallback().pp_completion_callback()));
      ASSERT_EQ(PP_OK, callback.result());
      ASSERT_TRUE(AreEqualWithString(var, text));
      ReleaseVar(var);
    }
    result = websocket_interface_->ReceiveMessage(
        ws, &var, callback.GetCallback().pp_completion_callback());
    core_interface_->ReleaseResource(ws);
    if (result != PP_OK) {
      callback.WaitForResult(result);
      ASSERT_EQ(PP_ERROR_ABORTED, callback.result());
    }
  }

  ReleaseVar(large_var);
  ReleaseVar(text_var);

  PASS();
}

std::string TestWebSocket::TestClosedFromServerWhileSending() {
  // Connect to test echo server.
  const pp::Var protocols[] = { pp::Var() };
  TestWebSocketAPI websocket(instance_);
  int32_t result =
      websocket.Connect(pp::Var(GetFullURL(kEchoServerURL)), protocols, 0U);
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  websocket.WaitForConnected();

  result = websocket.Send(pp::Var("hello"));
  ASSERT_EQ(PP_OK, result);
  result = websocket.Send(pp::Var("Goodbye"));
  // We send many messages so that PepperWebSocketHost::SendText is called
  // after PepperWebSocketHost::didClose is called.
  // Note: We must not wait for CLOSED event here because
  // WebSocketResource::SendMessage doesn't call PepperWebSocketHost::SendText
  // when its internal state is CLOSING or CLOSED. We want to test if the
  // pepper WebSocket works well when WebSocketResource is OPEN and
  // PepperWebSocketHost is CLOSED.
  for (size_t i = 0; i < 10000; ++i) {
    result = websocket.Send(pp::Var(""));
    ASSERT_EQ(PP_OK, result);
  }

  PASS();
}

std::string TestWebSocket::TestCcInterfaces() {
  // C++ bindings is simple straightforward, then just verifies interfaces work
  // as a interface bridge fine.
  pp::WebSocket ws(instance_);

  // Check uninitialized properties access.
  ASSERT_EQ(0, ws.GetBufferedAmount());
  ASSERT_EQ(0, ws.GetCloseCode());
  ASSERT_TRUE(AreEqualWithString(ws.GetCloseReason().pp_var(), std::string()));
  ASSERT_FALSE(ws.GetCloseWasClean());
  ASSERT_TRUE(AreEqualWithString(ws.GetExtensions().pp_var(), std::string()));
  ASSERT_TRUE(AreEqualWithString(ws.GetProtocol().pp_var(), std::string()));
  ASSERT_EQ(PP_WEBSOCKETREADYSTATE_INVALID, ws.GetReadyState());
  ASSERT_TRUE(AreEqualWithString(ws.GetURL().pp_var(), std::string()));

  // Check communication interfaces (connect, send, receive, and close).
  TestCompletionCallback connect_callback(
      instance_->pp_instance(), callback_type());
  connect_callback.WaitForResult(ws.Connect(
      pp::Var(GetFullURL(kCloseServerURL)), NULL, 0U,
              connect_callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(connect_callback);
  ASSERT_EQ(PP_OK, connect_callback.result());

  std::string text_message("hello C++");
  int32_t result = ws.SendMessage(pp::Var(text_message));
  ASSERT_EQ(PP_OK, result);

  std::vector<uint8_t> binary(256);
  for (uint32_t i = 0; i < binary.size(); ++i)
    binary[i] = i;
  result = ws.SendMessage(
      pp::Var(pp::PASS_REF, CreateVarBinary(binary)));
  ASSERT_EQ(PP_OK, result);

  pp::Var text_receive_var;
  TestCompletionCallback text_receive_callback(
      instance_->pp_instance(), callback_type());
  text_receive_callback.WaitForResult(
      ws.ReceiveMessage(&text_receive_var,
                        text_receive_callback.GetCallback()));
  ASSERT_EQ(PP_OK, text_receive_callback.result());
  ASSERT_TRUE(
      AreEqualWithString(text_receive_var.pp_var(), text_message.c_str()));

  pp::Var binary_receive_var;
  TestCompletionCallback binary_receive_callback(
      instance_->pp_instance(), callback_type());
  binary_receive_callback.WaitForResult(
      ws.ReceiveMessage(&binary_receive_var,
                        binary_receive_callback.GetCallback()));
  ASSERT_EQ(PP_OK, binary_receive_callback.result());
  ASSERT_TRUE(AreEqualWithBinary(binary_receive_var.pp_var(), binary));

  TestCompletionCallback close_callback(
      instance_->pp_instance(), callback_type());
  std::string reason("bye");
  close_callback.WaitForResult(ws.Close(
      PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, pp::Var(reason),
      close_callback.GetCallback()));
  CHECK_CALLBACK_BEHAVIOR(close_callback);
  ASSERT_EQ(PP_OK, close_callback.result());

  // Check initialized properties access.
  ASSERT_EQ(0, ws.GetBufferedAmount());
  ASSERT_EQ(PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, ws.GetCloseCode());
  ASSERT_TRUE(
      AreEqualWithString(ws.GetCloseReason().pp_var(), reason.c_str()));
  ASSERT_EQ(true, ws.GetCloseWasClean());
  ASSERT_TRUE(AreEqualWithString(ws.GetProtocol().pp_var(), std::string()));
  ASSERT_EQ(PP_WEBSOCKETREADYSTATE_CLOSED, ws.GetReadyState());
  ASSERT_TRUE(AreEqualWithString(
      ws.GetURL().pp_var(), GetFullURL(kCloseServerURL).c_str()));

  PASS();
}

std::string TestWebSocket::TestUtilityInvalidConnect() {
  const pp::Var protocols[] = { pp::Var() };

  TestWebSocketAPI websocket(instance_);
  int32_t result = websocket.Connect(pp::Var(), protocols, 1U);
  ASSERT_EQ(PP_ERROR_BADARGUMENT, result);
  ASSERT_EQ(0U, websocket.GetSeenEvents().size());

  result = websocket.Connect(pp::Var(), protocols, 1U);
  ASSERT_EQ(PP_ERROR_INPROGRESS, result);
  ASSERT_EQ(0U, websocket.GetSeenEvents().size());

  for (int i = 0; kInvalidURLs[i]; ++i) {
    TestWebSocketAPI ws(instance_);
    result = ws.Connect(pp::Var(std::string(kInvalidURLs[i])), protocols, 0U);
    if (result == PP_OK_COMPLETIONPENDING) {
      ws.WaitForClosed();
      const std::vector<WebSocketEvent>& events = ws.GetSeenEvents();
      ASSERT_EQ(WebSocketEvent::EVENT_ERROR, events[0].event_type);
      ASSERT_EQ(WebSocketEvent::EVENT_CLOSE, events[1].event_type);
      ASSERT_EQ(2U, ws.GetSeenEvents().size());
    } else {
      ASSERT_EQ(PP_ERROR_BADARGUMENT, result);
      ASSERT_EQ(0U, ws.GetSeenEvents().size());
    }
  }

  PASS();
}

std::string TestWebSocket::TestUtilityProtocols() {
  const pp::Var bad_protocols[] = {
      pp::Var(std::string("x-test")), pp::Var(std::string("x-test")) };
  const pp::Var good_protocols[] = {
      pp::Var(std::string("x-test")), pp::Var(std::string("x-yatest")) };

  {
    TestWebSocketAPI websocket(instance_);
    int32_t result = websocket.Connect(
        pp::Var(GetFullURL(kEchoServerURL)), bad_protocols, 2U);
    ASSERT_EQ(PP_ERROR_BADARGUMENT, result);
    ASSERT_EQ(0U, websocket.GetSeenEvents().size());
  }

  {
    TestWebSocketAPI websocket(instance_);
    int32_t result = websocket.Connect(
        pp::Var(GetFullURL(kEchoServerURL)), good_protocols, 2U);
    ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
    websocket.WaitForConnected();
    const std::vector<WebSocketEvent>& events = websocket.GetSeenEvents();
    // Protocol arguments are valid, but this test run without a WebSocket
    // server. As a result, OnError() and OnClose() are invoked because of
    // a connection establishment failure.
    ASSERT_EQ(2U, events.size());
    ASSERT_EQ(WebSocketEvent::EVENT_ERROR, events[0].event_type);
    ASSERT_EQ(WebSocketEvent::EVENT_CLOSE, events[1].event_type);
    ASSERT_FALSE(events[1].was_clean);
  }

  PASS();
}

std::string TestWebSocket::TestUtilityGetURL() {
  const pp::Var protocols[] = { pp::Var() };

  for (int i = 0; kInvalidURLs[i]; ++i) {
    TestWebSocketAPI websocket(instance_);
    int32_t result = websocket.Connect(
        pp::Var(std::string(kInvalidURLs[i])), protocols, 0U);
    if (result == PP_OK_COMPLETIONPENDING) {
      websocket.WaitForClosed();
      const std::vector<WebSocketEvent>& events = websocket.GetSeenEvents();
      ASSERT_EQ(WebSocketEvent::EVENT_ERROR, events[0].event_type);
      ASSERT_EQ(WebSocketEvent::EVENT_CLOSE, events[1].event_type);
      ASSERT_EQ(2U, events.size());
    } else {
      ASSERT_EQ(PP_ERROR_BADARGUMENT, result);
      ASSERT_EQ(0U, websocket.GetSeenEvents().size());
    }
    pp::Var url = websocket.GetURL();
    ASSERT_TRUE(AreEqualWithString(url.pp_var(), kInvalidURLs[i]));
  }

  PASS();
}

std::string TestWebSocket::TestUtilityValidConnect() {
  const pp::Var protocols[] = { pp::Var() };
  TestWebSocketAPI websocket(instance_);
  int32_t result = websocket.Connect(
      pp::Var(GetFullURL(kEchoServerURL)), protocols, 0U);
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  websocket.WaitForConnected();
  const std::vector<WebSocketEvent>& events = websocket.GetSeenEvents();
  ASSERT_EQ(1U, events.size());
  ASSERT_EQ(WebSocketEvent::EVENT_OPEN, events[0].event_type);
  ASSERT_TRUE(
      AreEqualWithString(websocket.GetExtensions().pp_var(), std::string()));

  PASS();
}

std::string TestWebSocket::TestUtilityInvalidClose() {
  const pp::Var reason = pp::Var(std::string("close for test"));

  // Close before connect.
  {
    TestWebSocketAPI websocket(instance_);
    int32_t result = websocket.Close(
        PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, reason);
    ASSERT_EQ(PP_ERROR_FAILED, result);
    ASSERT_EQ(0U, websocket.GetSeenEvents().size());
  }

  // Close with bad arguments.
  {
    TestWebSocketAPI websocket(instance_);
    int32_t result = websocket.Connect(pp::Var(GetFullURL(kEchoServerURL)),
        NULL, 0);
    ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
    websocket.WaitForConnected();
    result = websocket.Close(1U, reason);
    ASSERT_EQ(PP_ERROR_NOACCESS, result);
    const std::vector<WebSocketEvent>& events = websocket.GetSeenEvents();
    ASSERT_EQ(1U, events.size());
    ASSERT_EQ(WebSocketEvent::EVENT_OPEN, events[0].event_type);
  }

  PASS();
}

std::string TestWebSocket::TestUtilityValidClose() {
  std::string reason("close for test");
  pp::Var url = pp::Var(GetFullURL(kCloseServerURL));

  // Close.
  {
    TestWebSocketAPI websocket(instance_);
    int32_t result = websocket.Connect(url, NULL, 0U);
    ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
    websocket.WaitForConnected();
    result = websocket.Close(
        PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, pp::Var(reason));
    ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
    websocket.WaitForClosed();
    const std::vector<WebSocketEvent>& events = websocket.GetSeenEvents();
    ASSERT_EQ(2U, events.size());
    ASSERT_EQ(WebSocketEvent::EVENT_OPEN, events[0].event_type);
    ASSERT_EQ(WebSocketEvent::EVENT_CLOSE, events[1].event_type);
    ASSERT_TRUE(events[1].was_clean);
    ASSERT_EQ(PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, events[1].close_code);
    ASSERT_TRUE(AreEqualWithString(events[1].var.pp_var(), reason.c_str()));
  }

  // Close in connecting.
  // The ongoing connect failed with PP_ERROR_ABORTED, then the close is done
  // successfully.
  {
    TestWebSocketAPI websocket(instance_);
    int32_t result = websocket.Connect(url, NULL, 0U);
    ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
    result = websocket.Close(
        PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, pp::Var(reason));
    ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
    websocket.WaitForClosed();
    const std::vector<WebSocketEvent>& events = websocket.GetSeenEvents();
    ASSERT_TRUE(events.size() == 2 || events.size() == 3);
    int index = 0;
    if (events.size() == 3)
      ASSERT_EQ(WebSocketEvent::EVENT_OPEN, events[index++].event_type);
    ASSERT_EQ(WebSocketEvent::EVENT_ERROR, events[index++].event_type);
    ASSERT_EQ(WebSocketEvent::EVENT_CLOSE, events[index].event_type);
    ASSERT_FALSE(events[index].was_clean);
  }

  // Close in closing.
  // The first close will be done successfully, then the second one failed with
  // with PP_ERROR_INPROGRESS immediately.
  {
    TestWebSocketAPI websocket(instance_);
    int32_t result = websocket.Connect(url, NULL, 0U);
    result = websocket.Close(
        PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, pp::Var(reason));
    ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
    result = websocket.Close(
        PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, pp::Var(reason));
    ASSERT_EQ(PP_ERROR_INPROGRESS, result);
    websocket.WaitForClosed();
    const std::vector<WebSocketEvent>& events = websocket.GetSeenEvents();
    ASSERT_TRUE(events.size() == 2 || events.size() == 3);
    int index = 0;
    if (events.size() == 3)
      ASSERT_EQ(WebSocketEvent::EVENT_OPEN, events[index++].event_type);
    ASSERT_EQ(WebSocketEvent::EVENT_ERROR, events[index++].event_type);
    ASSERT_EQ(WebSocketEvent::EVENT_CLOSE, events[index].event_type);
    ASSERT_FALSE(events[index].was_clean);
  }

  PASS();
}

std::string TestWebSocket::TestUtilityGetProtocol() {
  const std::string protocol("x-chat");
  const pp::Var protocols[] = { pp::Var(protocol) };
  std::string url(GetFullURL(kProtocolTestServerURL));
  url += protocol;
  TestWebSocketAPI websocket(instance_);
  int32_t result = websocket.Connect(pp::Var(url), protocols, 1U);
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  websocket.WaitForReceived();
  ASSERT_TRUE(AreEqualWithString(
      websocket.GetProtocol().pp_var(), protocol.c_str()));
  const std::vector<WebSocketEvent>& events = websocket.GetSeenEvents();
  // The server to which this test connect returns the decided protocol as a
  // text frame message. So the WebSocketEvent records EVENT_MESSAGE event
  // after EVENT_OPEN event.
  ASSERT_EQ(2U, events.size());
  ASSERT_EQ(WebSocketEvent::EVENT_OPEN, events[0].event_type);
  ASSERT_EQ(WebSocketEvent::EVENT_MESSAGE, events[1].event_type);
  ASSERT_TRUE(AreEqualWithString(events[1].var.pp_var(), protocol.c_str()));

  PASS();
}

std::string TestWebSocket::TestUtilityTextSendReceive() {
  const pp::Var protocols[] = { pp::Var() };
  TestWebSocketAPI websocket(instance_);
  int32_t result =
      websocket.Connect(pp::Var(GetFullURL(kEchoServerURL)), protocols, 0U);
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  websocket.WaitForConnected();

  // Send 'hello pepper'.
  std::string message1("hello pepper");
  result = websocket.Send(pp::Var(std::string(message1)));
  ASSERT_EQ(PP_OK, result);

  // Receive echoed 'hello pepper'.
  websocket.WaitForReceived();

  // Send 'goodbye pepper'.
  std::string message2("goodbye pepper");
  result = websocket.Send(pp::Var(std::string(message2)));

  // Receive echoed 'goodbye pepper'.
  websocket.WaitForReceived();

  const std::vector<WebSocketEvent>& events = websocket.GetSeenEvents();
  ASSERT_EQ(3U, events.size());
  ASSERT_EQ(WebSocketEvent::EVENT_OPEN, events[0].event_type);
  ASSERT_EQ(WebSocketEvent::EVENT_MESSAGE, events[1].event_type);
  ASSERT_TRUE(AreEqualWithString(events[1].var.pp_var(), message1.c_str()));
  ASSERT_EQ(WebSocketEvent::EVENT_MESSAGE, events[2].event_type);
  ASSERT_TRUE(AreEqualWithString(events[2].var.pp_var(), message2.c_str()));

  PASS();
}

std::string TestWebSocket::TestUtilityBinarySendReceive() {
  const pp::Var protocols[] = { pp::Var() };
  TestWebSocketAPI websocket(instance_);
  int32_t result =
      websocket.Connect(pp::Var(GetFullURL(kEchoServerURL)), protocols, 0U);
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  websocket.WaitForConnected();

  // Send binary message.
  uint32_t len = 256;
  std::vector<uint8_t> binary(len);
  for (uint32_t i = 0; i < len; ++i)
    binary[i] = i;
  pp::VarArrayBuffer message(len);
  uint8_t* var_data = static_cast<uint8_t*>(message.Map());
  std::copy(binary.begin(), binary.end(), var_data);
  result = websocket.Send(message);
  ASSERT_EQ(PP_OK, result);

  // Receive echoed binary message.
  websocket.WaitForReceived();

  const std::vector<WebSocketEvent>& events = websocket.GetSeenEvents();
  ASSERT_EQ(2U, events.size());
  ASSERT_EQ(WebSocketEvent::EVENT_OPEN, events[0].event_type);
  ASSERT_EQ(WebSocketEvent::EVENT_MESSAGE, events[1].event_type);
  ASSERT_TRUE(AreEqualWithBinary(events[1].var.pp_var(), binary));

  PASS();
}

std::string TestWebSocket::TestUtilityBufferedAmount() {
  // Connect to test echo server.
  const pp::Var protocols[] = { pp::Var() };
  TestWebSocketAPI websocket(instance_);
  int32_t result =
      websocket.Connect(pp::Var(GetFullURL(kEchoServerURL)), protocols, 0U);
  ASSERT_EQ(PP_OK_COMPLETIONPENDING, result);
  websocket.WaitForConnected();

  // Prepare a large message that is not aligned with the internal buffer
  // sizes.
  std::string message(8193, 'x');
  uint64_t buffered_amount = 0;
  uint32_t sent;
  for (sent = 0; sent < 100; sent++) {
    result = websocket.Send(pp::Var(message));
    ASSERT_EQ(PP_OK, result);
    buffered_amount = websocket.GetBufferedAmount();
    // Buffered amount size 262144 is too big for the internal buffer size.
    if (buffered_amount > 262144)
      break;
  }

  // Close connection.
  std::string reason = "close while busy";
  result = websocket.Close(
      PP_WEBSOCKETSTATUSCODE_NORMAL_CLOSURE, pp::Var(reason));
  ASSERT_EQ(PP_WEBSOCKETREADYSTATE_CLOSING, websocket.GetReadyState());
  websocket.WaitForClosed();
  ASSERT_EQ(PP_WEBSOCKETREADYSTATE_CLOSED, websocket.GetReadyState());

  uint64_t base_buffered_amount = websocket.GetBufferedAmount();
  size_t events_on_closed = websocket.GetSeenEvents().size();

  // After connection closure, all sending requests fail and just increase
  // the bufferedAmount property.
  result = websocket.Send(pp::Var(std::string()));
  ASSERT_EQ(PP_ERROR_FAILED, result);
  buffered_amount = websocket.GetBufferedAmount();
  ASSERT_EQ(base_buffered_amount + kMessageFrameOverhead, buffered_amount);
  base_buffered_amount = buffered_amount;

  result = websocket.Send(pp::Var(reason));
  ASSERT_EQ(PP_ERROR_FAILED, result);
  buffered_amount = websocket.GetBufferedAmount();
  uint64_t reason_frame_size = kMessageFrameOverhead + reason.length();
  ASSERT_EQ(base_buffered_amount + reason_frame_size, buffered_amount);

  const std::vector<WebSocketEvent>& events = websocket.GetSeenEvents();
  ASSERT_EQ(events_on_closed, events.size());
  ASSERT_EQ(WebSocketEvent::EVENT_OPEN, events[0].event_type);
  size_t last_event = events_on_closed - 1;
  for (uint32_t i = 1; i < last_event; ++i) {
    ASSERT_EQ(WebSocketEvent::EVENT_MESSAGE, events[i].event_type);
    ASSERT_TRUE(AreEqualWithString(events[i].var.pp_var(), message));
  }
  ASSERT_EQ(WebSocketEvent::EVENT_CLOSE, events[last_event].event_type);
  ASSERT_TRUE(events[last_event].was_clean);

  PASS();
}
