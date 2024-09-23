// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/photos/photos_api.h"

#import "base/notreached.h"
#import "ios/chrome/browser/photos/model/photos_service.h"

class ChromiumPhotosService final : public PhotosService {
 public:
  ChromiumPhotosService() = default;
  ~ChromiumPhotosService() final = default;

  bool IsSupported() const final { return false; }
  bool IsAvailable() const final { return false; }

  void UploadImage(NSString* image_name,
                   NSData* image_data,
                   id<SystemIdentity> identity,
                   UploadProgressCallback progress_callback,
                   UploadCompletionCallback completion_callback) final {
    NOTREACHED();
  }

  void CancelUpload() final { NOTREACHED(); }
};

namespace ios {
namespace provider {

std::unique_ptr<PhotosService> CreatePhotosService(
    PhotosServiceConfiguration* configuration) {
  // Save to Photos is not supported.
  return std::make_unique<ChromiumPhotosService>();
}

}  // namespace provider
}  // namespace ios
