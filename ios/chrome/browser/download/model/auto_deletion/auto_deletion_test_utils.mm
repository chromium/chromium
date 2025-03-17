// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/auto_deletion/auto_deletion_test_utils.h"

#import <memory>

#import "base/time/time.h"
#import "ios/chrome/browser/download/model/auto_deletion/scheduled_file.h"

namespace {

// Test file name.
const base::FilePath::CharType kTestFileName[] =
    FILE_PATH_LITERAL("test_filepath.txt");
// Arbitrary hash string.
const std::string kTestHashValue = "test_hash_value";

}  // namespace

void PopulateSchedulerWithAutoDeletionSchedule(
    auto_deletion::Scheduler& scheduler,
    base::TimeDelta start_point_in_past,
    unsigned int number_of_files) {
  const base::Time now = base::Time::Now();
  for (unsigned int i = 0; i < number_of_files; i++) {
    base::TimeDelta download_time_offset =
        start_point_in_past + base::Days(number_of_files - i);
    DCHECK_GT(download_time_offset, base::Seconds(0));
    base::Time download_time = now - download_time_offset;

    base::FilePath path = base::FilePath(kTestFileName);
    auto_deletion::ScheduledFile file(path, kTestHashValue, download_time);
    scheduler.ScheduleFile(std::move(file));
  }
}
