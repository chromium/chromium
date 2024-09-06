// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/test_drive_service.h"

#import "ios/chrome/browser/drive/model/test_drive_file_uploader.h"
#import "ios/chrome/browser/drive/model/test_drive_list.h"

namespace drive {

TestDriveService::TestDriveService() = default;
TestDriveService::~TestDriveService() = default;

#pragma mark - Public

void TestDriveService::SetFileUploader(
    std::unique_ptr<DriveFileUploader> uploader) {
  file_uploader_ = std::move(uploader);
}

void TestDriveService::SetDriveList(std::unique_ptr<DriveList> list) {
  drive_list_ = std::move(list);
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

std::unique_ptr<DriveFileDownloader> TestDriveService::CreateFileDownloader(
    id<SystemIdentity> identity) {
  // TODO(crbug.com/344812969): Return a TestDriveList which fakes API requests,
  // similarly to TestDriveFileUploader.
  return nullptr;
}

std::unique_ptr<DriveList> TestDriveService::CreateList(
    id<SystemIdentity> identity) {
  std::unique_ptr<DriveList> result = std::move(drive_list_);
  if (!result) {
    result = std::make_unique<TestDriveList>(identity);
  }
  return result;
}

std::string TestDriveService::GetSuggestedFolderName() const {
  return std::string("test_drive_folder");
}

}  // namespace drive
