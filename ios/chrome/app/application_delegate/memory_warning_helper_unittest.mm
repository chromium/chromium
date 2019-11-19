// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/memory_warning_helper.h"

#include "base/bind.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#import "ios/chrome/browser/metrics/previous_session_info.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using previous_session_info_constants::
    kDidSeeMemoryWarningShortlyBeforeTerminating;

class MemoryWarningHelperTest : public PlatformTest {
 protected:
  MemoryWarningHelperTest() {
    // Set up |memory_pressure_listener_| to invoke |OnMemoryPressure| which
    // will store the memory pressure level sent to the callback in
    // |memory_pressure_level_| so that tests can verify the level is correct.
    memory_pressure_listener_.reset(new base::MemoryPressureListener(base::Bind(
        &MemoryWarningHelperTest::OnMemoryPressure, base::Unretained(this))));
    memory_pressure_level_ =
        base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE;
  }

  base::MemoryPressureListener::MemoryPressureLevel GetMemoryPressureLevel() {
    return memory_pressure_level_;
  }

  MemoryWarningHelper* GetMemoryHelper() {
    if (!memory_helper_) {
      memory_helper_ = [[MemoryWarningHelper alloc] init];
    }
    return memory_helper_;
  }

  // Callback for |memory_pressure_listener_|.
  void OnMemoryPressure(
      base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
    memory_pressure_level_ = memory_pressure_level;
    run_loop_.QuitWhenIdle();
  }

  void RunMessageLoop() { run_loop_.Run(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::RunLoop run_loop_;
  base::MemoryPressureListener::MemoryPressureLevel memory_pressure_level_;
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;
  MemoryWarningHelper* memory_helper_;

  DISALLOW_COPY_AND_ASSIGN(MemoryWarningHelperTest);
};

// Invokes resetForegroundMemoryWarningCount and verifies the
// foregroundMemoryWarningCount is setted to 0.
TEST_F(MemoryWarningHelperTest, VerifyForegroundMemoryWarningCountReset) {
  // Setup.
  [GetMemoryHelper() handleMemoryPressure];
  ASSERT_TRUE(GetMemoryHelper().foregroundMemoryWarningCount != 0);

  // Action.
  [GetMemoryHelper() resetForegroundMemoryWarningCount];

  // Test.
  EXPECT_EQ(0, GetMemoryHelper().foregroundMemoryWarningCount);
}

// Invokes applicationDidReceiveMemoryWarning and verifies the memory pressure
// callback (i.e. MainControllerTest::OnMemoryPressure) is invoked.
TEST_F(MemoryWarningHelperTest, VerifyApplicationDidReceiveMemoryWarning) {
  [GetMemoryHelper() handleMemoryPressure];
  RunMessageLoop();
  EXPECT_EQ(base::MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL,
            GetMemoryPressureLevel());
}

// Invokes applicationDidReceiveMemoryWarning and verifies the flags (i.e.
// breakpad_helper and NSUserDefaults) are set.
TEST_F(MemoryWarningHelperTest, VerifyHelperDidSetMemoryWarningFlags) {
  // Setup.
  [[PreviousSessionInfo sharedInstance] beginRecordingCurrentSession];
  [[PreviousSessionInfo sharedInstance] resetMemoryWarningFlag];
  int foregroundMemoryWarningCountBeforeWarning =
      GetMemoryHelper().foregroundMemoryWarningCount;

  BOOL memoryWarningFlagBeforeAlert = [[NSUserDefaults standardUserDefaults]
      boolForKey:kDidSeeMemoryWarningShortlyBeforeTerminating];

  // Action.
  [GetMemoryHelper() handleMemoryPressure];

  // Tests.
  EXPECT_TRUE([[NSUserDefaults standardUserDefaults]
      boolForKey:kDidSeeMemoryWarningShortlyBeforeTerminating]);
  EXPECT_FALSE(memoryWarningFlagBeforeAlert);
  EXPECT_EQ(foregroundMemoryWarningCountBeforeWarning + 1,
            GetMemoryHelper().foregroundMemoryWarningCount);
}
