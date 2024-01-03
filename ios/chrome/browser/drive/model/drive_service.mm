// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/drive_service.h"

#import <string>

namespace drive {

DriveService::DriveService() = default;

DriveService::~DriveService() = default;

// TODO(crbug.com/1495354): Remove implementation once subclasses provide their
// own.
std::unique_ptr<DriveFileUploader> DriveService::CreateFileUploader(
    id<SystemIdentity> identity) {
  return nullptr;
}

// TODO(crbug.com/1495354): Remove implementation once subclasses provide their
// own.
std::string DriveService::GetSuggestedFolderName() const {
  return std::string("Save to Drive folder");
}

}  // namespace drive
