// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/drive/drive_api.h"

#import "ios/chrome/browser/drive/model/test_drive_service.h"

namespace ios::provider {

std::unique_ptr<drive::DriveService> CreateDriveService(
    const drive::DriveServiceConfiguration& configuration) {
  return std::make_unique<drive::TestDriveService>();
}

}  // namespace ios::provider
