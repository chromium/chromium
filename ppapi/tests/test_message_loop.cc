// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_message_loop.h"

#include "ppapi/c/pp_macros.h"
#include "ppapi/cpp/core.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/message_loop.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/testing_instance.h"
#include "ppapi/utility/threading/simple_thread.h"

REGISTER_TEST_CASE(MessageLoop);

TestMessageLoop::TestMessageLoop(TestingInstance* instance)
    : TestCase(instance),
      param_(kInvalid),
      callback_factory_(this),
      main_loop_task_ran_(instance->pp_instance()) {
}

TestMessageLoop::~TestMessageLoop() {
}

void TestMessageLoop::RunTests(const std::string& filter) {
  RUN_TEST(Basics, filter);
  RUN_TEST(Post, filter);
}

std::string TestMessageLoop::TestBasics() {
  // The main thread message loop should be valid, and equal to the "current"
  // one.
  ASSERT_NE(0, pp::MessageLoop::GetForMainThread().pp_resource());
  ASSERT_EQ(pp::MessageLoop::GetForMainThread().pp_resource(),
            pp::MessageLoop::GetCurrent().pp_resource());

  // We shouldn't be able to attach a new loop to the main thread.
  pp::MessageLoop loop(instance_);
  ASSERT_EQ(PP_ERROR_INPROGRESS, loop.AttachToCurrentThread());

  // Nested loops aren't allowed.
  ASSERT_EQ(PP_ERROR_INPROGRESS,
            pp::MessageLoop::GetForMainThread().Run());

  // We can't run on a loop that isn't attached to a thread.
  ASSERT_EQ(PP_ERROR_WRONG_THREAD, loop.Run());

  PASS();
}

std::string TestMessageLoop::TestPost() {
  // Make sure we can post a task from the main thread back to the main thread.
  pp::MessageLoop::GetCurrent().PostWork(callback_factory_.NewCallback(
      &TestMessageLoop::SetParamAndQuitTask, kMainToMain));
  main_loop_task_ran_.Wait();
  ASSERT_EQ(param_, kMainToMain);
  main_loop_task_ran_.Reset();

  pp::SimpleThread thread(instance_);
  // Post a task before the thread is started, to make sure it is run.
  // TODO(dmichael): CompletionCallbackFactory is not 100% thread safe for
  // posting tasks to a thread other than where the factory was created. It
  // should be OK for this test, since we know that the
  // CompletionCallbackFactory and its target object outlive all callbacks. But
  // developers are likely to misuse CompletionCallbackFactory. Maybe we should
  // make it safe to use a callback on another thread?
  thread.message_loop().PostWork(callback_factory_.NewCallback(
      &TestMessageLoop::EchoParamToMainTask, kBeforeStart));
  ASSERT_TRUE(thread.Start());
  main_loop_task_ran_.Wait();
  ASSERT_EQ(param_, kBeforeStart);
  main_loop_task_ran_.Reset();

  // Now post another one after start. This is the more normal case.

  // Nested loops aren't allowed.
  ASSERT_EQ(PP_ERROR_INPROGRESS,
            pp::MessageLoop::GetForMainThread().Run());
  thread.message_loop().PostWork(callback_factory_.NewCallback(
      &TestMessageLoop::EchoParamToMainTask, kAfterStart));
  main_loop_task_ran_.Wait();
  ASSERT_EQ(param_, kAfterStart);
  main_loop_task_ran_.Reset();

  // Quit and join the thread.
  ASSERT_TRUE(thread.Join());

  PASS();
}

void TestMessageLoop::SetParamAndQuitTask(int32_t result, TestParam param) {
  PP_DCHECK(result == PP_OK);
  param_ = param;
  main_loop_task_ran_.Signal();
}

void TestMessageLoop::EchoParamToMainTask(int32_t result, TestParam param) {
  PP_DCHECK(result == PP_OK);
  pp::MessageLoop::GetForMainThread().PostWork(
      callback_factory_.NewCallback(
          &TestMessageLoop::SetParamAndQuitTask, param));
}

