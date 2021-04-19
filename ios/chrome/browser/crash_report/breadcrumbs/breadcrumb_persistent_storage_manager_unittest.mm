// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/breadcrumbs/core/breadcrumb_persistent_storage_manager.h"

#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#import "base/test/ios/wait_util.h"
#include "components/breadcrumbs/core/breadcrumb_manager_keyed_service.h"
#include "components/breadcrumbs/core/breadcrumb_persistent_storage_util.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_manager_keyed_service_factory.h"
#include "ios/chrome/browser/crash_report/breadcrumbs/breadcrumb_persistent_storage_util.h"
#import "ios/chrome/browser/crash_report/crash_reporter_breadcrumb_observer.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#include "ios/web/public/test/web_task_environment.h"
#include "testing/platform_test.h"

using base::test::ios::kWaitForFileOperationTimeout;
using base::test::ios::WaitUntilConditionOrTimeout;

namespace {

// Estimate number of events too large to fit in the persisted file. 6 is based
// on the event string format which is slightly smaller than each event.
constexpr unsigned long kEventCountTooManyForPersisting =
    breadcrumbs::kPersistedFilesizeInBytes / 6.0;

// Creates a new BreadcrumbManagerKeyedService for |browser_state|.
std::unique_ptr<KeyedService> BuildBreadcrumbManagerKeyedService(
    web::BrowserState* browser_state) {
  return std::make_unique<breadcrumbs::BreadcrumbManagerKeyedService>(
      browser_state->IsOffTheRecord());
}

// Validates that the events in |persisted_events| are contiguous and that the
// |last_logged_event| matches the last persisted event.
bool ValidatePersistedEvents(std::string last_logged_event,
                             std::vector<std::string> persisted_events) {
  if (persisted_events.empty()) {
    return false;
  }

  std::string last_event = persisted_events.back();
  if (last_event.find(last_logged_event) == std::string::npos) {
    return false;
  }

  std::string oldest_event = persisted_events.front();
  int first_event_index;
  if (!base::StringToInt(
          oldest_event.substr(oldest_event.find_last_of(' ') + 1),
          &first_event_index)) {
    // first_event_index could not be parsed.
    return false;
  }

  std::string newest_event = persisted_events.back();
  int last_event_index;
  if (!base::StringToInt(
          newest_event.substr(newest_event.find_last_of(' ') + 1),
          &last_event_index)) {
    // last_event_index could not be parsed.
    return false;
  }

  // Validate no events are missing from within the persisted range.
  return last_event_index - first_event_index + 1 ==
         static_cast<int>(persisted_events.size());
}

}  // namespace

class BreadcrumbPersistentStorageManagerTest : public PlatformTest {
 protected:
  BreadcrumbPersistentStorageManagerTest()
      : scoped_browser_state_manager_(
            std::make_unique<TestChromeBrowserStateManager>(base::FilePath())) {
    EXPECT_TRUE(scoped_temp_directory_.CreateUniqueTempDir());
    base::FilePath directory_name = scoped_temp_directory_.GetPath();

    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.SetPath(directory_name);
    test_cbs_builder.AddTestingFactory(
        BreadcrumbManagerKeyedServiceFactory::GetInstance(),
        base::BindRepeating(&BuildBreadcrumbManagerKeyedService));
    chrome_browser_state_ = test_cbs_builder.Build();

    breadcrumb_manager_service_ =
        static_cast<breadcrumbs::BreadcrumbManagerKeyedService*>(
            BreadcrumbManagerKeyedServiceFactory::GetForBrowserState(
                chrome_browser_state_.get()));
    persistent_storage_ =
        std::make_unique<breadcrumbs::BreadcrumbPersistentStorageManager>(
            directory_name,
            breadcrumb_persistent_storage_util::
                GetOldBreadcrumbPersistentStorageFilePath(directory_name),
            breadcrumb_persistent_storage_util::
                GetOldBreadcrumbPersistentStorageTempFilePath(directory_name));
    breadcrumb_manager_service_->StartPersisting(persistent_storage_.get());
  }

  ~BreadcrumbPersistentStorageManagerTest() override {
    breadcrumb_manager_service_->StopPersisting();
  }

  web::WebTaskEnvironment task_env_{
      web::WebTaskEnvironment::Options::DEFAULT,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  IOSChromeScopedTestingChromeBrowserStateManager scoped_browser_state_manager_;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;
  base::ScopedTempDir scoped_temp_directory_;
  breadcrumbs::BreadcrumbManagerKeyedService* breadcrumb_manager_service_;
  std::unique_ptr<breadcrumbs::BreadcrumbPersistentStorageManager>
      persistent_storage_;
};

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Ensures that logged events are persisted.
TEST_F(BreadcrumbPersistentStorageManagerTest, PersistEvents) {
  breadcrumb_manager_service_->AddEvent("event");

  // Advance clock to trigger writing final events.
  task_env_.FastForwardBy(base::TimeDelta::FromMinutes(1));

  __block bool events_received = false;
  persistent_storage_->GetStoredEvents(
      base::BindOnce(^(std::vector<std::string> events) {
        ASSERT_EQ(1ul, events.size());
        EXPECT_NE(std::string::npos, events.front().find("event"));
        events_received = true;
      }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return events_received;
  }));
}

// Ensures that persisted events do not grow too large for a single large event
// bucket when events are logged very quickly one after the other.
TEST_F(BreadcrumbPersistentStorageManagerTest, PersistLargeBucket) {
  std::string event;
  unsigned long event_count = 0;
  while (event_count < kEventCountTooManyForPersisting) {
    event = base::StringPrintf("event %lu", event_count);
    breadcrumb_manager_service_->AddEvent(event);
    task_env_.FastForwardBy(base::TimeDelta::FromMilliseconds(1));

    event_count++;
  }

  // Advance clock to trigger writing final events.
  task_env_.FastForwardBy(base::TimeDelta::FromMinutes(1));

  __block bool events_received = false;
  persistent_storage_->GetStoredEvents(
      base::BindOnce(^(std::vector<std::string> events) {
        EXPECT_LT(events.size(), event_count);

        EXPECT_TRUE(ValidatePersistedEvents(event, events));
        events_received = true;
      }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return events_received;
  }));
}

// Ensures that persisted events do not grow too large for events logged a few
// seconds apart from each other.
TEST_F(BreadcrumbPersistentStorageManagerTest, PersistManyEventsOverTime) {
  std::string event;
  unsigned long event_count = 0;
  while (event_count < kEventCountTooManyForPersisting) {
    event = base::StringPrintf("event %lu", event_count);
    breadcrumb_manager_service_->AddEvent(event);
    task_env_.FastForwardBy(base::TimeDelta::FromSeconds(1));

    event_count++;
  }

  // Advance clock to trigger writing final events.
  task_env_.FastForwardBy(base::TimeDelta::FromMinutes(1));

  __block bool events_received = false;
  persistent_storage_->GetStoredEvents(
      base::BindOnce(^(std::vector<std::string> events) {
        ASSERT_GT(events.size(), 0ul);
        EXPECT_LT(events.size(), event_count);

        EXPECT_TRUE(ValidatePersistedEvents(event, events));

        events_received = true;
      }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return events_received;
  }));
}

// Ensures that old events are removed from the persisted file when old buckets
// are dropped.
TEST_F(BreadcrumbPersistentStorageManagerTest,
       OldEventsRemovedFromPersistedFile) {
  std::string event;
  unsigned long event_counter = 0;
  const int kNumEventsPerBucket = 200;
  while (event_counter < kNumEventsPerBucket * 3) {
    event = base::StringPrintf("event %lu", event_counter);
    breadcrumb_manager_service_->AddEvent(event);
    event_counter++;

    if (event_counter % kNumEventsPerBucket == 0) {
      task_env_.FastForwardBy(base::TimeDelta::FromHours(1));
    }
  }

  // Advance clock to trigger writing final events.
  task_env_.FastForwardBy(base::TimeDelta::FromMinutes(1));

  __block bool events_received = false;
  persistent_storage_->GetStoredEvents(
      base::BindOnce(^(std::vector<std::string> events) {
        // The exact number of events could vary based on changes in the
        // implementation. The important part of this test is to verify that a
        // single event bucket will not grow unbounded and it will be limited to
        // a value smaller than the overall total number of events which have
        // been logged.
        EXPECT_LT(events.size(), event_counter);

        EXPECT_TRUE(ValidatePersistedEvents(event, events));

        events_received = true;
      }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return events_received;
  }));
}

// Ensures that events are read correctly if the persisted file becomes
// corrupted by losing the EOF token or if kPersistedFilesizeInBytes is
// reduced.
TEST_F(BreadcrumbPersistentStorageManagerTest,
       GetStoredEventsAfterFilesizeReduction) {
  const base::FilePath breadcrumbs_file_path =
      breadcrumbs::GetBreadcrumbPersistentStorageFilePath(
          scoped_temp_directory_.GetPath());

  auto file = std::make_unique<base::File>(
      breadcrumbs_file_path,
      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  ASSERT_TRUE(file->IsValid());

  // Simulate an old persisted file larger than the current one.
  const size_t old_filesize = breadcrumbs::kPersistedFilesizeInBytes * 1.2;
  std::string past_breadcrumbs;
  unsigned long written_events = 0;
  while (past_breadcrumbs.length() < old_filesize) {
    past_breadcrumbs += "08:27 event\n";
    written_events++;
  }

  ASSERT_TRUE(file->WriteAndCheck(
      /*offset=*/0, base::as_bytes(base::make_span(past_breadcrumbs))));
  ASSERT_TRUE(file->Flush());
  file->Close();

  __block bool events_received = false;
  persistent_storage_->GetStoredEvents(
      base::BindOnce(^(std::vector<std::string> events) {
        EXPECT_GT(events.size(), 1ul);
        EXPECT_LT(events.size(), written_events);
        events_received = true;
      }));

  ASSERT_TRUE(WaitUntilConditionOrTimeout(kWaitForFileOperationTimeout, ^{
    base::RunLoop().RunUntilIdle();
    return events_received;
  }));
}

using BreadcrumbPersistentStorageManagerFilenameTest = PlatformTest;

TEST_F(BreadcrumbPersistentStorageManagerFilenameTest,
       MigrateOldBreadcrumbFiles) {
  web::WebTaskEnvironment task_env;
  base::ScopedTempDir scoped_temp_directory;
  ASSERT_TRUE(scoped_temp_directory.CreateUniqueTempDir());
  base::FilePath directory_name = scoped_temp_directory.GetPath();

  // Create breadcrumb file and temp file with old filenames.
  const base::FilePath old_breadcrumb_file_path =
      breadcrumb_persistent_storage_util::
          GetOldBreadcrumbPersistentStorageFilePath(directory_name);
  const base::FilePath old_temp_file_path = breadcrumb_persistent_storage_util::
      GetOldBreadcrumbPersistentStorageTempFilePath(directory_name);
  base::File old_breadcrumb_file(
      old_breadcrumb_file_path,
      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  base::File old_temp_file(old_temp_file_path, base::File::FLAG_CREATE_ALWAYS |
                                                   base::File::FLAG_WRITE);
  ASSERT_TRUE(old_breadcrumb_file.IsValid());
  ASSERT_TRUE(old_temp_file.IsValid());
  old_temp_file.Close();

  // Write some test data to the breadcrumb file.
  const std::string test_data = "breadcrumb file test data";
  ASSERT_NE(-1,
            old_breadcrumb_file.Write(0, test_data.c_str(), test_data.size()));
  old_breadcrumb_file.Close();

  breadcrumbs::BreadcrumbPersistentStorageManager persistent_storage(
      directory_name, old_breadcrumb_file_path, old_temp_file_path);
  task_env.RunUntilIdle();

  // The old files should have been removed, and the new breadcrumb file should
  // be present.
  EXPECT_FALSE(base::PathExists(old_breadcrumb_file_path));
  EXPECT_FALSE(base::PathExists(old_temp_file_path));
  base::File new_file(
      breadcrumbs::GetBreadcrumbPersistentStorageFilePath(directory_name),
      base::File::FLAG_OPEN | base::File::FLAG_READ);
  EXPECT_TRUE(new_file.IsValid());
  const size_t test_data_size = test_data.size();
  char new_file_data[test_data_size];
  EXPECT_EQ(static_cast<int>(test_data_size),
            new_file.Read(0, new_file_data, test_data_size));
  EXPECT_EQ(test_data, std::string(new_file_data, test_data_size));
}
