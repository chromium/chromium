// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CONTROLLER_TESTS_BLINK_TEST_SUITE_H_
#define THIRD_PARTY_BLINK_RENDERER_CONTROLLER_TESTS_BLINK_TEST_SUITE_H_

#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "content/public/test/blink_test_environment.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_gc_controller.h"
#include "third_party/blink/renderer/platform/heap/thread_state.h"
#include "v8/include/v8.h"

template <class Parent>
class BlinkUnitTestSuite : public Parent {
 public:
  BlinkUnitTestSuite(int argc, char** argv) : Parent(argc, argv) {}

  BlinkUnitTestSuite(const BlinkUnitTestSuite&) = delete;
  BlinkUnitTestSuite& operator=(const BlinkUnitTestSuite&) = delete;

 private:
  void Initialize() override {
    Parent::Initialize();

    content::SetUpBlinkTestEnvironment();
  }
  void Shutdown() override {
    // Tickle EndOfTaskRunner which among other things will flush the queue
    // of error messages via V8Initializer::reportRejectedPromisesOnMainThread.
    blink::scheduler::GetSingleThreadTaskRunnerForTesting()->PostTask(
        FROM_HERE, base::DoNothing());
    base::RunLoop().RunUntilIdle();

    // Collect garbage (including threadspecific persistent handles) in order
    // to release mock objects referred from v8 or Oilpan heap. Otherwise false
    // mock leaks will be reported.
    blink::ThreadState::Current()->CollectAllGarbageForTesting();

    content::TearDownBlinkTestEnvironment();

    Parent::Shutdown();
  }
};

#endif  // THIRD_PARTY_BLINK_RENDERER_CONTROLLER_TESTS_BLINK_TEST_SUITE_H_
