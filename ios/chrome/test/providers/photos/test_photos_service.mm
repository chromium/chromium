// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/providers/photos/test_photos_service.h"

#import "base/task/sequenced_task_runner.h"

TestPhotosService::TestPhotosService() = default;
TestPhotosService::~TestPhotosService() = default;

void TestPhotosService::SetUploadResult(UploadResult result) {
  result_ = result;
}

void TestPhotosService::SetQuitClosure(base::RepeatingClosure quit_closure) {
  quit_closure_ = std::move(quit_closure);
}

NSString* TestPhotosService::GetImageName() {
  return image_name_;
}

NSData* TestPhotosService::GetImageData() {
  return image_data_;
}

id<SystemIdentity> TestPhotosService::GetIdentity() {
  return identity_;
}

#pragma mark - PhotosService

bool TestPhotosService::IsSupported() const {
  return true;
}

bool TestPhotosService::IsAvailable() const {
  return !cancelable_upload_completion_.callback();
}

void TestPhotosService::UploadImage(
    NSString* image_name,
    NSData* image_data,
    id<SystemIdentity> identity,
    UploadProgressCallback progress_callback,
    UploadCompletionCallback completion_callback) {
  image_name_ = [image_name copy];
  image_data_ = [image_data copy];
  identity_ = identity;
  cancelable_upload_completion_.Reset(
      base::BindOnce(std::move(completion_callback), result_));
  int64_t number_of_bytes = static_cast<int64_t>(image_data.length);
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          progress_callback,
          UploadProgress{.bytes_sent = number_of_bytes,
                         .total_bytes_sent = number_of_bytes,
                         .total_bytes_expected_to_send = number_of_bytes})
          .Then(cancelable_upload_completion_.callback())
          .Then(quit_closure_));
}

void TestPhotosService::CancelUpload() {
  if (!cancelable_upload_completion_.IsCancelled()) {
    cancelable_upload_completion_.Cancel();
  }
}
