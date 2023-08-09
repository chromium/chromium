// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/photos/photos_api.h"

#import "base/cancelable_callback.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/photos/photos_service.h"

class TestPhotosService final : public PhotosService {
 public:
  TestPhotosService() = default;
  ~TestPhotosService() final = default;

  bool IsAvailable() const final {
    return !cancelable_upload_completion_.callback();
  }

  void UploadImage(NSString* image_name,
                   NSData* image_data,
                   id<SystemIdentity> identity,
                   UploadProgressCallback progress_callback,
                   UploadCompletionCallback completion_callback) final {
    UploadResult result;
    result.successful = true;
    cancelable_upload_completion_.Reset(
        base::BindOnce(std::move(completion_callback), result));
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, cancelable_upload_completion_.callback());
  }

  void CancelUpload() final {
    if (!cancelable_upload_completion_.IsCancelled()) {
      cancelable_upload_completion_.Cancel();
    }
  }

 private:
  base::CancelableOnceClosure cancelable_upload_completion_;
};

namespace ios {
namespace provider {

std::unique_ptr<PhotosService> CreatePhotosService(
    PhotosServiceConfiguration* configuration) {
  return std::make_unique<TestPhotosService>();
}

}  // namespace provider
}  // namespace ios
