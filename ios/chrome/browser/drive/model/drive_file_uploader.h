// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_FILE_UPLOADER_H_
#define IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_FILE_UPLOADER_H_

// This interface is used to perform queries in a user's Drive account.
class DriveFileUploader {
 public:
  DriveFileUploader();
  virtual ~DriveFileUploader();
};

#endif  // IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_FILE_UPLOADER_H_
