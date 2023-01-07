// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_WEBSOCKET_H_
#define PPAPI_TESTS_TEST_WEBSOCKET_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppb_var_array_buffer.h"
#include "ppapi/c/ppb_websocket.h"
#include "ppapi/tests/test_case.h"

class TestWebSocket : public TestCase {
 public:
  explicit TestWebSocket(TestingInstance* instance) : TestCase(instance) {}

 private:
  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

  std::string GetFullURL(const char* url);
  PP_Var CreateVarString(const std::string& string);
  PP_Var CreateVarBinary(const std::vector<uint8_t>& binary);
  void ReleaseVar(const PP_Var& var);
  bool AreEqualWithString(const PP_Var& var, const std::string& string);
  bool AreEqualWithBinary(const PP_Var& var,
                          const std::vector<uint8_t>& binary);

  PP_Resource Connect(const std::string& url,
                      int32_t* result,
                      const std::string& protocol);

  void Send(int32_t result, PP_Resource ws, const std::string& message);

  std::string TestIsWebSocket();
  std::string TestUninitializedPropertiesAccess();
  std::string TestInvalidConnect();
  std::string TestProtocols();
  std::string TestGetURL();
  std::string TestValidConnect();
  std::string TestInvalidClose();
  std::string TestValidClose();
  std::string TestGetProtocol();
  std::string TestTextSendReceive();
  std::string TestTextSendReceiveTwice();
  std::string TestBinarySendReceive();
  std::string TestStressedSendReceive();
  std::string TestBufferedAmount();
  std::string TestAbortCallsWithCallback();
  std::string TestAbortSendMessageCall();
  std::string TestAbortCloseCall();
  std::string TestAbortReceiveMessageCall();
  std::string TestClosedFromServerWhileSending();

  std::string TestCcInterfaces();

  std::string TestUtilityInvalidConnect();
  std::string TestUtilityProtocols();
  std::string TestUtilityGetURL();
  std::string TestUtilityValidConnect();
  std::string TestUtilityInvalidClose();
  std::string TestUtilityValidClose();
  std::string TestUtilityGetProtocol();
  std::string TestUtilityTextSendReceive();
  std::string TestUtilityBinarySendReceive();
  std::string TestUtilityBufferedAmount();

  // Keeps Pepper API interfaces. These are used by the tests that access the C
  // API directly.
  const PPB_WebSocket* websocket_interface_;
  const PPB_Var* var_interface_;
  const PPB_VarArrayBuffer* arraybuffer_interface_;
  const PPB_Core* core_interface_;
};

#endif  // PPAPI_TESTS_TEST_WEBSOCKET_H_
