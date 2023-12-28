// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/drive/drive_api.h"

#import "ios/chrome/browser/drive/model/drive_service.h"

namespace {

class ChromiumDriveService final : public drive::DriveService {
 public:
  ChromiumDriveService() = default;
  ~ChromiumDriveService() final = default;

  bool IsSupported() const final { return false; }
};

}  // namespace

namespace ios::provider {

std::unique_ptr<drive::DriveService> CreateDriveService(
    const drive::DriveServiceConfiguration& configuration) {
  // Save to Drive is not supported.
  return std::make_unique<ChromiumDriveService>();
}

}  // namespace ios::provider
