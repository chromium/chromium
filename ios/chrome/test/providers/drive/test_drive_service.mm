// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/providers/drive/test_drive_service.h"

namespace drive {

TestDriveService::TestDriveService() = default;
TestDriveService::~TestDriveService() = default;

#pragma mark - DriveService

bool TestDriveService::IsSupported() const {
  return true;
}

}  // namespace drive
