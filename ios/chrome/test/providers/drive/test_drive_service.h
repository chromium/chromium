// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_PROVIDERS_DRIVE_TEST_DRIVE_SERVICE_H_
#define IOS_CHROME_TEST_PROVIDERS_DRIVE_TEST_DRIVE_SERVICE_H_

#import "ios/chrome/browser/drive/model/drive_service.h"

namespace drive {

class TestDriveService final : public DriveService {
 public:
  TestDriveService();
  ~TestDriveService() final;

  // DriveService implementation
  bool IsSupported() const final;
};

}  // namespace drive

#endif  // IOS_CHROME_TEST_PROVIDERS_DRIVE_TEST_DRIVE_SERVICE_H_
