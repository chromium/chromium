// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/auto_deletion/scheduler.h"

#import "base/time/time.h"
#import "components/prefs/pref_service.h"
#import "components/prefs/testing_pref_service.h"
#import "ios/chrome/browser/download/model/auto_deletion/auto_deletion_test_utils.h"
#import "ios/chrome/browser/download/model/auto_deletion/scheduled_file.h"
#import "ios/chrome/browser/shared/model/prefs/browser_prefs.h"
#import "ios/chrome/browser/shared/model/prefs/pref_names.h"
#import "ios/chrome/test/testing_application_context.h"
#import "testing/platform_test.h"

namespace auto_deletion {

class SchedulerTest : public PlatformTest {
 protected:
  SchedulerTest() {
    local_state_ = std::make_unique<TestingPrefServiceSimple>();
    RegisterLocalStatePrefs(local_state_->registry());
    TestingApplicationContext::GetGlobal()->SetLocalState(local_state());
  }

  void TearDown() override {
    TestingApplicationContext::GetGlobal()->SetLocalState(nullptr);
    local_state_.reset();
    PlatformTest::TearDown();
  }

  PrefService* local_state() { return local_state_.get(); }
  const base::Value::List& scheduled_files() {
    return local_state_->GetList(prefs::kDownloadAutoDeletionScheduledFiles);
  }

 private:
  std::unique_ptr<TestingPrefServiceSimple> local_state_;
};

// Tests that files that are scheduled less than 30 days ago will not be
// identified for deletion.
TEST_F(SchedulerTest, IdentifyNoFilesForDeletion) {
  base::TimeDelta start_point_in_past = base::Days(0);
  size_t number_of_files = 10;
  auto_deletion::Scheduler scheduler(local_state());
  PopulateSchedulerWithAutoDeletionSchedule(scheduler, start_point_in_past,
                                            number_of_files);

  std::vector<ScheduledFile> files_for_deletion =
      scheduler.IdentifyExpiredFiles(base::Time::Now());

  EXPECT_EQ(files_for_deletion.size(), 0u);
}

// Tests that files that are scheduled earlier than 30 days ago will not be
// identified for deletion.
TEST_F(SchedulerTest, IdentifyEveryFileForDeletion) {
  base::TimeDelta start_point_in_past = base::Days(30);
  size_t number_of_files = 10;
  auto_deletion::Scheduler scheduler(local_state());
  PopulateSchedulerWithAutoDeletionSchedule(scheduler, start_point_in_past,
                                            number_of_files);

  std::vector<ScheduledFile> files_for_deletion =
      scheduler.IdentifyExpiredFiles(base::Time::Now());

  EXPECT_EQ(files_for_deletion.size(), 10u);
}

}  // namespace auto_deletion
