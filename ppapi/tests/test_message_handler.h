// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_MESSAGE_HANDLER_H_
#define PPAPI_TESTS_TEST_MESSAGE_HANDLER_H_

#include <string>

#include "ppapi/c/ppb_messaging.h"
#include "ppapi/tests/test_case.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/utility/threading/simple_thread.h"

class TestMessageHandler : public TestCase {
 public:
  explicit TestMessageHandler(TestingInstance* instance);
  virtual ~TestMessageHandler();

 private:
  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);
  virtual void HandleMessage(const pp::Var& message_data);

  std::string TestRegisterErrorConditions();
  std::string TestPostMessageAndAwaitResponse();
  std::string TestArrayBuffer();
  std::string TestExceptions();

  // Wait for HandleMessage to be called, and return the Var it received.
  pp::Var WaitForMessage();
  NestedEvent message_received_;
  pp::Var last_message_;

  const PPB_Messaging_1_2* ppb_messaging_if_;
  pp::SimpleThread handler_thread_;

};

#endif  // PPAPI_TESTS_TEST_MESSAGE_HANDLER_H_

