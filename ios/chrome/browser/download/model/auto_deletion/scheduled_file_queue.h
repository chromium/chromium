// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_MODEL_AUTO_DELETION_SCHEDULED_FILE_QUEUE_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_MODEL_AUTO_DELETION_SCHEDULED_FILE_QUEUE_H_

#import <deque>

#import "base/values.h"
#import "ios/chrome/browser/download/model/auto_deletion/scheduled_file.h"

namespace auto_deletion::proto {
class ScheduledFile;
class ScheduleFileList;
}  // namespace auto_deletion::proto

namespace auto_deletion {

// A queue of ScheduleFile objects that have been scheduled for automatic
// deletion by the user. The files in the queue are sorted by expiration date
// with the files closest to their expiration date in front.
class ScheduledFileQueue {
 public:
  ScheduledFileQueue();
  ScheduledFileQueue(const ScheduledFileQueue& other);
  ScheduledFileQueue& operator=(const ScheduledFileQueue& other);
  ScheduledFileQueue(ScheduledFileQueue&& other);
  ScheduledFileQueue& operator=(ScheduledFileQueue&& other);
  ~ScheduledFileQueue();

  // Static method that creates a ScheduledFileQueue and populates it with the
  // values contained in the given auto_deletion::proto::ScheduledFileList
  // proto.
  static ScheduledFileQueue LoadFromProto(
      const auto_deletion::proto::ScheduleFileList& proto);

  // Adds the file to the queue.
  void Enqueue(ScheduledFile file);

  // Removes the next file in the queue.
  ScheduledFile Dequeue();

  // Returns the number of files that have been scheduled.
  size_t Count() const;

  // Returns the scheduled file that is next in the queue.
  const ScheduledFile& Peek() const;

  // Converts the ScheduledFileQueue object into a ScheduleFileList proto.
  auto_deletion::proto::ScheduleFileList Serialize();

  // Operator overrides.
  friend bool operator==(const ScheduledFileQueue& lhs,
                         const ScheduledFileQueue rhs);
  friend bool operator!=(const ScheduledFileQueue& lhs,
                         const ScheduledFileQueue rhs);

 private:
  // The dequeue the class wraps around and treats as a queue.
  std::deque<ScheduledFile> scheduled_files_;
};

}  // namespace auto_deletion

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_MODEL_AUTO_DELETION_SCHEDULED_FILE_QUEUE_H_
