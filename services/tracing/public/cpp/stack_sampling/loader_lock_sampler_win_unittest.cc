// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/tracing/public/cpp/stack_sampling/loader_lock_sampler_win.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/scoped_native_library.h"
#include "base/strings/string16.h"
#include "base/task/thread_pool.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/test/test_waitable_event.h"
#include "base/win/scoped_handle.h"
#include "services/tracing/public/cpp/stack_sampling/loader_lock_sampler_test_strings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tracing {

namespace {

class TestNamedWaitableEvent : public base::TestWaitableEvent {
 public:
  TestNamedWaitableEvent(const base::char16* name)
      : TestWaitableEvent(
            base::win::ScopedHandle(::CreateEvent(nullptr,
                                                  /*bManualReset=*/TRUE,
                                                  /*bInitialState=*/FALSE,
                                                  name))) {}
};

}  // namespace

TEST(LoaderLockSamplerTest, LockNotHeld) {
  InitializeLoaderLockSampling();
  EXPECT_FALSE(IsLoaderLockHeld());
}

TEST(LoaderLockSamplerTest, LockHeldByOtherThread) {
  InitializeLoaderLockSampling();

  base::test::TaskEnvironment task_environment{
      base::test::TaskEnvironment::ThreadPoolCOMEnvironment::NONE};

  // Creating TaskEnvironment initializes the thread pool, which takes the
  // loader lock while creating background threads. Wait until it's released.
  while (IsLoaderLockHeld()) {
    base::TestWaitableEvent delay_event;
    delay_event.TimedWait(TestTimeouts::tiny_timeout());
  }

  // Create events with fixed names that can be accessed from the helper DLL.
  TestNamedWaitableEvent wait_for_loader_lock_event(
      loader_lock_sampler_test::kWaitForLockEventName);
  TestNamedWaitableEvent drop_loader_lock_event(
      loader_lock_sampler_test::kDropLockEventName);

  base::TestWaitableEvent dll_done_event;
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock()},
      base::BindLambdaForTesting([&dll_done_event] {
        // This function will block until DllMain returns, or the load fails.
        // The helper DLL will signal |wait_for_loader_lock_event| as soon as it
        // enters DllMain, while it holds the loader lock. Then it will block
        // with the loader lock held until |drop_loader_lock_event| is
        // signalled.
        base::ScopedNativeLibrary dll(base::FilePath(
            FILE_PATH_LITERAL("loader_lock_sampler_test_dll.dll")));

        // If the DLL did not load, |wait_for_loader_lock_event| will not be
        // signalled and the TimedWait below will return false (timeout).
        ASSERT_TRUE(dll.is_valid())
            << "ScopedNativeLibrary error " << dll.GetError()->ToString();

        dll_done_event.Signal();
      }));

  // Wait for the background thread to take the loader lock.
  ASSERT_TRUE(
      wait_for_loader_lock_event.TimedWait(TestTimeouts::action_timeout()));

  EXPECT_TRUE(IsLoaderLockHeld());

  // Tell the DLL to drop the loader lock and exit from DllMain.
  drop_loader_lock_event.Signal();

  // Make sure the DllMain has exited.
  ASSERT_TRUE(dll_done_event.TimedWait(TestTimeouts::action_timeout()));

  // It takes a moment for the lock to be released.
  base::TestWaitableEvent delay_event;
  delay_event.TimedWait(TestTimeouts::tiny_timeout());

  EXPECT_FALSE(IsLoaderLockHeld());
}

}  // namespace tracing
