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

  // Sets file uploader to be returned by `CreateFileUploader(...)`.
  void SetFileUploader(std::unique_ptr<DriveFileUploader> uploader);

  // DriveService implementation
  bool IsSupported() const final;
  std::unique_ptr<DriveFileUploader> CreateFileUploader(
      id<SystemIdentity> identity) final;
  std::string GetSuggestedFolderName() const final;

 private:
  std::unique_ptr<DriveFileUploader> file_uploader_;
};

}  // namespace drive

#endif  // IOS_CHROME_BROWSER_DRIVE_MODEL_TEST_DRIVE_SERVICE_H_
