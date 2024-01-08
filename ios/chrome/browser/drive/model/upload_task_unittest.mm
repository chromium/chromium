// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/upload_task.h"

#import "ios/chrome/browser/drive/model/test_upload_task_observer.h"
#import "testing/platform_test.h"

// Testing implementation of `UploadTask`.
class TestUploadTask final : public UploadTask {
 public:
  TestUploadTask() = default;
  ~TestUploadTask() final = default;

  void Update() { OnUploadUpdated(); }

  // UploadTask overrides.
  State GetState() const final { return State::kNotStarted; }
  void Start() final {}
  void Cancel() final {}
  id<SystemIdentity> GetIdentity() const final { return nil; }
  float GetProgress() const final { return 0; }
  NSURL* GetResponseLink() const final { return nil; }
  NSError* GetError() const final { return nil; }
};

// UploadTask unit tests.
class UploadTaskTest : public PlatformTest {};

// Tests that `UploadTask` notifies all observing `UploadTaskObserver` instances
// when the task is updated and when it is destroyed.
TEST_F(UploadTaskTest, NotifiesObservers) {
  TestUploadTaskObserver observer;
  TestUploadTask* task_ptr = nullptr;
  {
    TestUploadTask task;
    task_ptr = &task;
    task.AddObserver(&observer);
    EXPECT_EQ(nullptr, observer.GetUpdatedUpload());
    EXPECT_EQ(nullptr, observer.GetDestroyedUpload());
    task.Update();
    EXPECT_EQ(task_ptr, observer.GetUpdatedUpload());
    EXPECT_EQ(nullptr, observer.GetDestroyedUpload());
    observer.ResetUpdatedUpload();
  }
  EXPECT_EQ(nullptr, observer.GetUpdatedUpload());
  EXPECT_EQ(task_ptr, observer.GetDestroyedUpload());
}
