// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/photos/photos_api.h"

#import "ios/chrome/test/providers/photos/test_photos_service.h"

namespace ios {
namespace provider {

std::unique_ptr<PhotosService> CreatePhotosService(
    PhotosServiceConfiguration* configuration) {
  return std::make_unique<TestPhotosService>();
}

}  // namespace provider
}  // namespace ios
