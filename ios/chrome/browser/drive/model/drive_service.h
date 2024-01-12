// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_SERVICE_H_
#define IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_SERVICE_H_

#import <memory>

#import "components/keyed_service/core/keyed_service.h"

class DriveFileUploader;
@class NSString;
@protocol SystemIdentity;

namespace drive {

// Service responsible for providing access to the Drive API.
class DriveService : public KeyedService {
 public:
  DriveService();
  ~DriveService() override;

  // Whether the service is supported. This value does not change during the
  // execution of the application.
  virtual bool IsSupported() const = 0;

  // Returns a DriveFileUploader to perform queries on the Drive of `identity`.
  // TODO(crbug.com/1495354): Remove implementation once subclasses provide
  // their own.
  virtual std::unique_ptr<DriveFileUploader> CreateFileUploader(
      id<SystemIdentity> identity);

  // Returns a name suggestion for the folder in which to add uploaded files.
  // TODO(crbug.com/1495354): Remove implementation once subclasses provide
  // their own.
  virtual std::string GetSuggestedFolderName() const;
};

}  // namespace drive

#endif  // IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_SERVICE_H_
