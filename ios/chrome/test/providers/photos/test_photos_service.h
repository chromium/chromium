// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_PROVIDERS_PHOTOS_TEST_PHOTOS_SERVICE_H_
#define IOS_CHROME_TEST_PROVIDERS_PHOTOS_TEST_PHOTOS_SERVICE_H_

#import "base/cancelable_callback.h"
#import "base/functional/callback_helpers.h"
#import "ios/chrome/browser/photos/model/photos_service.h"

class TestPhotosService final : public PhotosService {
 public:
  TestPhotosService();
  ~TestPhotosService() final;

  // Set result given to upload completion
  void SetUploadResult(UploadResult result);
  void SetQuitClosure(base::RepeatingClosure quit_closure);
  NSString* GetImageName();
  NSData* GetImageData();
  id<SystemIdentity> GetIdentity();

  // PhotosService implementation
  bool IsSupported() const final;
  bool IsAvailable() const final;
  void UploadImage(NSString* image_name,
                   NSData* image_data,
                   id<SystemIdentity> identity,
                   UploadProgressCallback progress_callback,
                   UploadCompletionCallback completion_callback) final;
  void CancelUpload() final;

 private:
  base::CancelableOnceClosure cancelable_upload_completion_;
  UploadResult result_ = {.successful = true};
  base::RepeatingClosure quit_closure_ = base::DoNothing();
  NSString* image_name_;
  NSData* image_data_;
  id<SystemIdentity> identity_;
};

#endif  // IOS_CHROME_TEST_PROVIDERS_PHOTOS_TEST_PHOTOS_SERVICE_H_
