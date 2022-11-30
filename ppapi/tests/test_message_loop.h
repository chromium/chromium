// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_MESSAGE_LOOP_H_
#define PPAPI_TESTS_TEST_MESSAGE_LOOP_H_

#include <string>

#include "ppapi/c/pp_stdint.h"
#include "ppapi/tests/test_case.h"
#include "ppapi/utility/completion_callback_factory.h"

class TestMessageLoop : public TestCase {
 public:
  explicit TestMessageLoop(TestingInstance* instance);
  virtual ~TestMessageLoop();

 private:
  // TestCase implementation.
  virtual void RunTests(const std::string& filter);

  std::string TestBasics();
  std::string TestPost();

  // This just ensures we have a unique number for each little thing we test.
  enum TestParam { kInvalid, kMainToMain, kBeforeStart, kAfterStart};

  // A task to run on the main thread. It sets param_ and quits the main loop.
  void SetParamAndQuitTask(int32_t result, TestParam param);

  // A task to run on a background thread. It posts SetResultAndQuitTask to the
  // main loop, echoing result.
  void EchoParamToMainTask(int32_t result, TestParam param);

  // The last test param we received in SetParamAndQuitTask (or kInvalid if
  // none).
  TestParam param_;
  pp::CompletionCallbackFactory<TestMessageLoop> callback_factory_;
  NestedEvent main_loop_task_ran_;
};

#endif  // PPAPI_TESTS_TEST_MESSAGE_LOOP_H_
