// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/auto_deletion/scheduled_file_queue.h"

#import "ios/chrome/browser/download/model/auto_deletion/proto/scheduled_file.pb.h"

namespace auto_deletion {

ScheduledFileQueue::ScheduledFileQueue() {}

ScheduledFileQueue::ScheduledFileQueue(const ScheduledFileQueue&) = default;
ScheduledFileQueue& ScheduledFileQueue::operator=(const ScheduledFileQueue&) =
    default;
ScheduledFileQueue::ScheduledFileQueue(ScheduledFileQueue&&) = default;
ScheduledFileQueue& ScheduledFileQueue::operator=(ScheduledFileQueue&&) =
    default;
ScheduledFileQueue::~ScheduledFileQueue() = default;

ScheduledFileQueue ScheduledFileQueue::LoadFromProto(
    const auto_deletion::proto::ScheduleFileList& proto) {
  ScheduledFileQueue queue;

  for (const proto::ScheduledFile& message : proto.files()) {
    std::optional<ScheduledFile> scheduled_file =
        ScheduledFile::Deserialize(message);
    if (scheduled_file) {
      queue.Enqueue(std::move(*scheduled_file));
    }
  }

  return queue;
}

void ScheduledFileQueue::Enqueue(ScheduledFile file) {
  scheduled_files_.push_back(std::move(file));
}

ScheduledFile ScheduledFileQueue::Dequeue() {
  CHECK(!scheduled_files_.empty());
  ScheduledFile file = scheduled_files_.front();
  scheduled_files_.pop_front();
  return file;
}

size_t ScheduledFileQueue::Count() const {
  return scheduled_files_.size();
}

const ScheduledFile& ScheduledFileQueue::Peek() const {
  CHECK(!scheduled_files_.empty());
  return scheduled_files_.front();
}

auto_deletion::proto::ScheduleFileList ScheduledFileQueue::Serialize() {
  auto_deletion::proto::ScheduleFileList list;

  for (const ScheduledFile& file : scheduled_files_) {
    auto_deletion::proto::ScheduledFile proto = file.Serialize();
    *list.add_files() = proto;
  }

  return list;
}

bool operator==(const ScheduledFileQueue& lhs, const ScheduledFileQueue rhs) {
  return lhs.scheduled_files_ == rhs.scheduled_files_;
}

bool operator!=(const ScheduledFileQueue& lhs, const ScheduledFileQueue rhs) {
  return lhs.scheduled_files_ != rhs.scheduled_files_;
}

}  // namespace auto_deletion
