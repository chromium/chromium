// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_MODEL_TEST_DRIVE_SERVICE_H_
#define IOS_CHROME_BROWSER_DRIVE_MODEL_TEST_DRIVE_SERVICE_H_

#import "ios/chrome/browser/drive/model/drive_service.h"

namespace drive {

class TestDriveService final : public DriveService {
 public:
  TestDriveService();
  ~TestDriveService() final;

  // Sets file downloader to be returned by `CreateFileDownloader(...)`.
  void SetFileDownloader(std::unique_ptr<DriveFileDownloader> downloader);
  // Sets file uploader to be returned by `CreateFileUploader(...)`.
  void SetFileUploader(std::unique_ptr<DriveFileUploader> uploader);
  // Sets Drive list object to be returned by `CreateList(...)`.
  void SetDriveList(std::unique_ptr<DriveList> list);

  // DriveService implementation
  bool IsSupported() const final;
  std::unique_ptr<DriveFileUploader> CreateFileUploader(
      id<SystemIdentity> identity) final;
  std::unique_ptr<DriveFileDownloader> CreateFileDownloader(
      id<SystemIdentity> identity) final;
  std::unique_ptr<DriveList> CreateList(id<SystemIdentity> identity) final;
  std::string GetSuggestedFolderName() const final;

 private:
  std::unique_ptr<DriveFileDownloader> file_downloader_;
  std::unique_ptr<DriveFileUploader> file_uploader_;
  std::unique_ptr<DriveList> drive_list_;
};

}  // namespace drive

#endif  // IOS_CHROME_BROWSER_DRIVE_MODEL_TEST_DRIVE_SERVICE_H_
