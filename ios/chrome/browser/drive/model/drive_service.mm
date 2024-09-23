// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive/model/drive_service.h"

#import <string>

namespace drive {

DriveService::DriveService() = default;

DriveService::~DriveService() = default;

// TODO(crbug.com/344812086): Make this pure virtual once implemented
// everywhere.
std::unique_ptr<DriveFileDownloader> DriveService::CreateFileDownloader(
    id<SystemIdentity> identity) {
  return nullptr;
}

// TODO(crbug.com/344812086): Make this pure virtual once implemented
// everywhere.
std::unique_ptr<DriveList> DriveService::CreateList(
    id<SystemIdentity> identity) {
  return nullptr;
}

}  // namespace drive
