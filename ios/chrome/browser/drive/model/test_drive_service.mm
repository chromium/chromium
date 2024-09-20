// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/test_drive_service.h"

#import "ios/chrome/browser/drive/model/test_drive_file_downloader.h"
#import "ios/chrome/browser/drive/model/test_drive_file_uploader.h"
#import "ios/chrome/browser/drive/model/test_drive_list.h"

namespace drive {

TestDriveService::TestDriveService() = default;
TestDriveService::~TestDriveService() = default;

#pragma mark - Public

void TestDriveService::SetFileDownloader(
    std::unique_ptr<DriveFileDownloader> downloader) {
  file_downloader_ = std::move(downloader);
}

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
  std::unique_ptr<DriveFileDownloader> result = std::move(file_downloader_);
  if (!result) {
    result = std::make_unique<TestDriveFileDownloader>(identity);
  }
  return result;
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
