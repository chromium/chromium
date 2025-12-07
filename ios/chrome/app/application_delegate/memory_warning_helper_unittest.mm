// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_delegate/memory_warning_helper.h"

#import "base/functional/bind.h"
#import "base/memory/memory_pressure_listener.h"
#import "base/test/task_environment.h"
#import "base/threading/thread.h"
#import "components/previous_session_info/previous_session_info.h"
#import "testing/platform_test.h"

using previous_session_info_constants::
    kDidSeeMemoryWarningShortlyBeforeTerminating;

class MemoryWarningHelperTest : public PlatformTest,
                                public base::MemoryPressureListener {
 public:
  MemoryWarningHelperTest(const MemoryWarningHelperTest&) = delete;
  MemoryWarningHelperTest& operator=(const MemoryWarningHelperTest&) = delete;

 protected:
  MemoryWarningHelperTest() {
    // Set up `memory_pressure_listener_registration_` to invoke
    // `OnMemoryPressure` which will store the memory pressure level sent to the
    // callback in `memory_pressure_level_` so that tests can verify the level
    // is correct.
    memory_pressure_listener_registration_.reset(
        new base::SyncMemoryPressureListenerRegistration(
            base::MemoryPressureListenerTag::kTest, this));
    memory_pressure_level_ = base::MEMORY_PRESSURE_LEVEL_MODERATE;
  }

  base::MemoryPressureLevel GetMemoryPressureLevel() {
    return memory_pressure_level_;
  }

  MemoryWarningHelper* GetMemoryHelper() {
    if (!memory_helper_) {
      memory_helper_ = [[MemoryWarningHelper alloc] init];
    }
    return memory_helper_;
  }

  // Callback for `memory_pressure_listener_`.
  void OnMemoryPressure(
      base::MemoryPressureLevel memory_pressure_level) override {
    memory_pressure_level_ = memory_pressure_level;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::MemoryPressureLevel memory_pressure_level_;
  std::unique_ptr<base::SyncMemoryPressureListenerRegistration>
      memory_pressure_listener_registration_;
  MemoryWarningHelper* memory_helper_;
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
  EXPECT_EQ(base::MEMORY_PRESSURE_LEVEL_CRITICAL, GetMemoryPressureLevel());
}

// Invokes applicationDidReceiveMemoryWarning and verifies the flags (i.e.
// crash_helper and NSUserDefaults) are set.
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
