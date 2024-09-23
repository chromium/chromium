// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_SERVICE_H_
#define IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_SERVICE_H_

#import <memory>

#import "components/keyed_service/core/keyed_service.h"

class DriveFileDownloader;
class DriveFileUploader;
class DriveList;
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
  virtual std::unique_ptr<DriveFileUploader> CreateFileUploader(
      id<SystemIdentity> identity) = 0;

  // Returns a DriveFileDownloader to download items from the Drive of
  // `identity`.
  // TODO(crbug.com/344812086): Make this pure virtual once implemented
  // everywhere.
  virtual std::unique_ptr<DriveFileDownloader> CreateFileDownloader(
      id<SystemIdentity> identity);

  // Returns a DriveList to perform queries on the Drive of `identity`.
  // TODO(crbug.com/344812086): Make this pure virtual once implemented
  // everywhere.
  virtual std::unique_ptr<DriveList> CreateList(id<SystemIdentity> identity);

  // Returns a name suggestion for the folder in which to add uploaded files.
  virtual std::string GetSuggestedFolderName() const = 0;
};

}  // namespace drive

#endif  // IOS_CHROME_BROWSER_DRIVE_MODEL_DRIVE_SERVICE_H_
