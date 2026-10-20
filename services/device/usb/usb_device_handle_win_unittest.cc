// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/usb/usb_device_handle_win.h"

#include <windows.h>

#include "base/location.h"
#include "base/run_loop.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "base/test/test_timeouts.h"
#include "base/win/object_watcher.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {
namespace {

class UsbRequestEventTest : public ::testing::Test,
                            public base::win::ObjectWatcher::Delegate {
 public:
  void OnObjectSignaled(HANDLE object) override {
    ++callback_count_;
    // Use nonblocking waits so an auto-reset regression fails, not hangs.
    EXPECT_EQ(WAIT_OBJECT_0, WaitForSingleObject(object, 0));
    EXPECT_EQ(WAIT_OBJECT_0, WaitForSingleObject(object, 0));
    run_loop_.Quit();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  base::RunLoop run_loop_;
  int callback_count_ = 0;
};

TEST_F(UsbRequestEventTest, SignalBeforeWatching) {
  base::test::ScopedRunLoopTimeout timeout(FROM_HERE,
                                           TestTimeouts::action_timeout());
  auto event = CreateUsbRequestEvent();
  ASSERT_TRUE(event.is_valid());
  EXPECT_EQ(WAIT_TIMEOUT, WaitForSingleObject(event.Get(), 0));

  base::win::ObjectWatcher watcher;
  ASSERT_TRUE(SetEvent(event.Get()));
  ASSERT_TRUE(watcher.StartWatchingOnce(event.Get(), this));

  run_loop_.Run();
  EXPECT_FALSE(watcher.IsWatching());
  EXPECT_EQ(1, callback_count_);

  auto next_event = CreateUsbRequestEvent();
  ASSERT_TRUE(next_event.is_valid());
  EXPECT_EQ(WAIT_TIMEOUT, WaitForSingleObject(next_event.Get(), 0));
}

TEST_F(UsbRequestEventTest, SignalAfterWatching) {
  base::test::ScopedRunLoopTimeout timeout(FROM_HERE,
                                           TestTimeouts::action_timeout());
  auto event = CreateUsbRequestEvent();
  ASSERT_TRUE(event.is_valid());
  EXPECT_EQ(WAIT_TIMEOUT, WaitForSingleObject(event.Get(), 0));

  base::win::ObjectWatcher watcher;
  ASSERT_TRUE(watcher.StartWatchingOnce(event.Get(), this));
  ASSERT_TRUE(SetEvent(event.Get()));

  run_loop_.Run();
  EXPECT_FALSE(watcher.IsWatching());
  EXPECT_EQ(1, callback_count_);

  auto next_event = CreateUsbRequestEvent();
  ASSERT_TRUE(next_event.is_valid());
  EXPECT_EQ(WAIT_TIMEOUT, WaitForSingleObject(next_event.Get(), 0));
}

}  // namespace
}  // namespace device
