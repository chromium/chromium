// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_AUTO_DELETION_SCHEDULER_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_AUTO_DELETION_SCHEDULER_H_

#import "base/memory/raw_ptr.h"

namespace auto_deletion {
class ScheduledFile;
}  // namespace auto_deletion
namespace base {
class Time;
}  // namespace base
class PrefService;

namespace auto_deletion {

// A class that is responsible for managing which downloaded files are enrolled
// for auto-deletion and when they should be deleted. This class is not
// responsible for the actual deletion of the files.
class Scheduler {
 public:
  Scheduler(PrefService* local_state);
  ~Scheduler();
  // The Scheduler is non-copyable and non-movable.
  Scheduler(const Scheduler&) = delete;
  Scheduler& operator=(const Scheduler&) = delete;
  Scheduler(Scheduler&&) = delete;
  Scheduler& operator=(Scheduler&&) = delete;

  // Returns a list of files whose scheduled deletion dates have elapsed.
  [[nodiscard]] std::vector<ScheduledFile> IdentifyExpiredFiles(
      base::Time instant);

  // Removes the ScheduledFiles whose deletion dates have elased.
  void RemoveExpiredFiles(base::Time instant);

  // Schedules the file for deletion.
  void ScheduleFile(ScheduledFile file);

  // Removes all the ScheduledFiles regardless of deletion date.
  void Clear();

 private:
  // Returns whether the file is older than one month.
  bool IsFileReadyForDeletion(base::Time instant, const ScheduledFile& file);

  // The PrefService where the list of ScheduledFiles awaiting automatic
  // deletion is stored.
  raw_ptr<PrefService> local_state_;
};

}  // namespace auto_deletion

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_AUTO_DELETION_SCHEDULER_H_
