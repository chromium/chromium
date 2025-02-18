// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/auto_deletion/scheduled_file_queue.h"

#import "base/time/time.h"
#import "base/values.h"
#import "ios/chrome/browser/download/model/auto_deletion/proto/scheduled_file.pb.h"
#import "ios/chrome/browser/download/model/auto_deletion/scheduled_file.h"
#import "testing/platform_test.h"

namespace {

// Test file name.
const base::FilePath::CharType kTestFileName[] =
    FILE_PATH_LITERAL("test_filepath.txt");
// Arbitrary hash string.
const std::string kTestHashValue = "test_hash_value";

const auto_deletion::ScheduledFile CreateTestScheduledFile() {
  base::FilePath path = base::FilePath(kTestFileName);
  const auto_deletion::ScheduledFile file(path, kTestHashValue,
                                          base::Time::Now());
  return file;
}

auto_deletion::proto::ScheduleFileList CreateSerializedQueue() {
  auto_deletion::ScheduledFileQueue queue;
  for (int i = 0; i < 10; i++) {
    auto file = CreateTestScheduledFile();
    queue.Enqueue(std::move(file));
  }

  return queue.Serialize();
}

}  // namespace

namespace auto_deletion {

class ScheduledFileQueueTest : public PlatformTest {};

TEST_F(ScheduledFileQueueTest, intializeEmptyQueue) {
  ScheduledFileQueue queue;

  EXPECT_EQ(queue.Count(), 0u);
}

TEST_F(ScheduledFileQueueTest, initializeQueueRestoration) {
  auto_deletion::proto::ScheduleFileList proto = CreateSerializedQueue();

  ScheduledFileQueue queue = ScheduledFileQueue::LoadFromProto(proto);

  EXPECT_EQ(queue.Count(), 10u);
}

TEST_F(ScheduledFileQueueTest, AddFileToEmptyQueue) {
  auto file = CreateTestScheduledFile();
  ScheduledFileQueue queue;
  queue.Enqueue(std::move(file));

  EXPECT_EQ(queue.Count(), 1u);
}

TEST_F(ScheduledFileQueueTest, AddFileToRestoredQueue) {
  auto_deletion::proto::ScheduleFileList proto = CreateSerializedQueue();
  ScheduledFileQueue queue = ScheduledFileQueue::LoadFromProto(proto);

  queue.Enqueue(CreateTestScheduledFile());
  queue.Enqueue(CreateTestScheduledFile());
  queue.Enqueue(CreateTestScheduledFile());

  EXPECT_EQ(queue.Count(), 13u);
}

TEST_F(ScheduledFileQueueTest, QueueIsEquivalentWhenSerializedDataIsShared) {
  auto_deletion::proto::ScheduleFileList proto = CreateSerializedQueue();
  ScheduledFileQueue lhs = ScheduledFileQueue::LoadFromProto(proto);
  ScheduledFileQueue rhs = ScheduledFileQueue::LoadFromProto(proto);

  EXPECT_EQ(lhs, rhs);
}

TEST_F(ScheduledFileQueueTest, QueueIsNotEquivalentWhenOneQueueIsShorter) {
  auto_deletion::proto::ScheduleFileList proto = CreateSerializedQueue();
  ScheduledFileQueue lhs = ScheduledFileQueue::LoadFromProto(proto);
  ScheduledFileQueue rhs = ScheduledFileQueue::LoadFromProto(proto);

  lhs.Dequeue();

  EXPECT_NE(lhs, rhs);
}

TEST_F(ScheduledFileQueueTest,
       QueueIsNotEquivalentWhenQueuesContainDifferentData) {
  auto_deletion::proto::ScheduleFileList proto = CreateSerializedQueue();
  ScheduledFileQueue lhs = ScheduledFileQueue::LoadFromProto(proto);
  ScheduledFileQueue rhs = ScheduledFileQueue::LoadFromProto(proto);

  lhs.Dequeue();
  base::FilePath different_path =
      base::FilePath(FILE_PATH_LITERAL("different_filepath.txt"));
  auto_deletion::ScheduledFile file(different_path, "example_hash",
                                    base::Time::Now());
  lhs.Enqueue(std::move(file));

  ASSERT_NE(lhs, rhs);
}
}  // namespace auto_deletion
