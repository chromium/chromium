// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/drive_upload_task.h"

#import "testing/platform_test.h"

// DriveUploadTask unit tests.
class DriveUploadTaskTest : public PlatformTest {
 protected:
  void SetUp() final {
    PlatformTest::SetUp();
    task_ = std::make_unique<DriveUploadTask>();
  }

  std::unique_ptr<DriveUploadTask> task_;
};

// Tests that upon first starting and later cancelling an upload task, the state
// of the task is correctly updated.
TEST_F(DriveUploadTaskTest, TaskCanBeStartedAndCancelled) {
  using State = UploadTask::State;
  EXPECT_EQ(0, task_->GetProgress());
  EXPECT_EQ(State::kNotStarted, task_->GetState());
  EXPECT_EQ(nil, task_->GetError());
  EXPECT_EQ(nil, task_->GetResponseLink());
  EXPECT_FALSE(task_->IsDone());
  // Starting the task.
  task_->Start();
  EXPECT_EQ(State::kInProgress, task_->GetState());
  // Cancelling the task.
  task_->Cancel();
  EXPECT_EQ(State::kCancelled, task_->GetState());
  EXPECT_TRUE(task_->IsDone());
}
