// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_AUTO_DELETION_AUTO_DELETION_TEST_UTILS_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_AUTO_DELETION_AUTO_DELETION_TEST_UTILS_H_

#import "ios/chrome/browser/download/model/auto_deletion/scheduler.h"

namespace auto_deletion {
class Scheduler;
}  // namespace auto_deletion
namespace base {
class TimeDelta;
}  // namespace base

// Populates the scheduler object with ScheduledFiles following the given
// schedule config.
void PopulateSchedulerWithAutoDeletionSchedule(
    auto_deletion::Scheduler& scheduler,
    base::TimeDelta start_point_in_past,
    unsigned int number_of_files);

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_AUTO_DELETION_AUTO_DELETION_TEST_UTILS_H_
