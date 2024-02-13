// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/test_drive_service.h"

#import "ios/chrome/browser/drive/model/test_drive_file_uploader.h"

namespace drive {

TestDriveService::TestDriveService() = default;
TestDriveService::~TestDriveService() = default;

#pragma mark - Public

void TestDriveService::SetFileUploader(
    std::unique_ptr<DriveFileUploader> uploader) {
  file_uploader_ = std::move(uploader);
}

#pragma mark - DriveService

bool TestDriveService::IsSupported() const {
  return true;
}

std::unique_ptr<DriveFileUploader> TestDriveService::CreateFileUploader(
    id<SystemIdentity> identity) {
  std::unique_ptr<DriveFileUploader> result = std::move(file_uploader_);
  if (!result) {
    result = std::make_unique<TestDriveFileUploader>(identity);
  }
  return result;
}

std::string TestDriveService::GetSuggestedFolderName() const {
  return std::string("test_drive_folder");
}

}  // namespace drive
