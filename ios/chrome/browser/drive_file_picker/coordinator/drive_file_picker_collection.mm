// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/drive_file_picker/coordinator/drive_file_picker_collection.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

namespace {
// My Drive folder identifier. See
// https://developers.google.com/workspace/drive/api/guides/folder for details
// about folders in the Google Drive API.
NSString* const kMyDriveFolderIdentifier = @"root";
}  // namespace

#pragma mark - Static

std::unique_ptr<DriveFilePickerCollection> DriveFilePickerCollection::GetRoot(
    id<SystemIdentity> root_identity) {
  return std::make_unique<DriveFilePickerCollection>(
      root_identity, nil, DriveFilePickerCollectionType::kRoot, nil,
      std::nullopt);
}

#pragma mark - Constructor/destructor

DriveFilePickerCollection::DriveFilePickerCollection(
    id<SystemIdentity> identity,
    NSString* title,
    DriveFilePickerCollectionType type,
    NSString* folder_identifier,
    std::optional<DriveFilePickerFirstLevel> first_level)
    : identity_(identity),
      title_([title copy]),
      type_(type),
      folder_identifier_([folder_identifier copy]),
      first_level_(first_level) {
  CHECK(identity_);
  CHECK(type == DriveFilePickerCollectionType::kFolder || !folder_identifier_);
}

DriveFilePickerCollection::~DriveFilePickerCollection() = default;

#pragma mark - Public

bool DriveFilePickerCollection::IsRoot() const {
  return type_ == DriveFilePickerCollectionType::kRoot;
}

bool DriveFilePickerCollection::IsFolder() const {
  return type_ == DriveFilePickerCollectionType::kFolder;
}

bool DriveFilePickerCollection::IsSharedDrives() const {
  return type_ == DriveFilePickerCollectionType::kSharedDrives;
}

bool DriveFilePickerCollection::IsStarred() const {
  return type_ == DriveFilePickerCollectionType::kStarred;
}

bool DriveFilePickerCollection::IsRecent() const {
  return type_ == DriveFilePickerCollectionType::kRecent;
}

bool DriveFilePickerCollection::IsSharedWithMe() const {
  return type_ == DriveFilePickerCollectionType::kSharedWithMe;
}

bool DriveFilePickerCollection::SupportsFiltering() const {
  switch (type_) {
    case DriveFilePickerCollectionType::kRoot:
    case DriveFilePickerCollectionType::kRecent:
    case DriveFilePickerCollectionType::kSharedDrives:
      return false;
    default:
      return true;
  }
}

bool DriveFilePickerCollection::SupportsSorting() const {
  switch (type_) {
    case DriveFilePickerCollectionType::kRoot:
    case DriveFilePickerCollectionType::kRecent:
    case DriveFilePickerCollectionType::kSharedWithMe:
    case DriveFilePickerCollectionType::kSharedDrives:
      return false;
    default:
      return true;
  }
}

std::unique_ptr<DriveFilePickerCollection>
DriveFilePickerCollection::GetFirstLevelCollection(NSString* identifier) const {
  if ([identifier isEqual:kDriveFilePickerMyDriveItemIdentifier]) {
    return GetMyDrive();
  } else if ([identifier isEqual:kDriveFilePickerStarredItemIdentifier]) {
    return GetStarred();
  } else if ([identifier isEqual:kDriveFilePickerRecentItemIdentifier]) {
    return GetRecent();
  } else if ([identifier isEqual:kDriveFilePickerSharedWithMeItemIdentifier]) {
    return GetSharedWithMe();
  } else if ([identifier isEqual:kDriveFilePickerSharedDrivesItemIdentifier]) {
    return GetSharedDrives();
  }
  return nullptr;
}

std::unique_ptr<DriveFilePickerCollection>
DriveFilePickerCollection::GetMyDrive() const {
  return std::make_unique<DriveFilePickerCollection>(
      identity_, l10n_util::GetNSString(IDS_IOS_DRIVE_FILE_PICKER_MY_DRIVE),
      DriveFilePickerCollectionType::kFolder, kMyDriveFolderIdentifier,
      DriveFilePickerFirstLevel::kMyDrive);
}

std::unique_ptr<DriveFilePickerCollection> DriveFilePickerCollection::GetFolder(
    NSString* folder_title,
    NSString* folder_identifier_arg) const {
  return std::make_unique<DriveFilePickerCollection>(
      identity_, folder_title, DriveFilePickerCollectionType::kFolder,
      folder_identifier_arg, std::nullopt);
}

std::unique_ptr<DriveFilePickerCollection>
DriveFilePickerCollection::GetSharedDrives() const {
  return std::make_unique<DriveFilePickerCollection>(
      identity_,
      l10n_util::GetNSString(IDS_IOS_DRIVE_FILE_PICKER_SHARED_DRIVES),
      DriveFilePickerCollectionType::kSharedDrives, nil,
      DriveFilePickerFirstLevel::kSharedDrive);
}

std::unique_ptr<DriveFilePickerCollection>
DriveFilePickerCollection::GetStarred() const {
  return std::make_unique<DriveFilePickerCollection>(
      identity_, l10n_util::GetNSString(IDS_IOS_DRIVE_FILE_PICKER_STARRED),
      DriveFilePickerCollectionType::kStarred, nil,
      DriveFilePickerFirstLevel::kStarred);
}

std::unique_ptr<DriveFilePickerCollection>
DriveFilePickerCollection::GetRecent() const {
  return std::make_unique<DriveFilePickerCollection>(
      identity_, l10n_util::GetNSString(IDS_IOS_DRIVE_FILE_PICKER_RECENT),
      DriveFilePickerCollectionType::kRecent, nil,
      DriveFilePickerFirstLevel::kRecent);
}

std::unique_ptr<DriveFilePickerCollection>
DriveFilePickerCollection::GetSharedWithMe() const {
  return std::make_unique<DriveFilePickerCollection>(
      identity_,
      l10n_util::GetNSString(IDS_IOS_DRIVE_FILE_PICKER_SHARED_WITH_ME),
      DriveFilePickerCollectionType::kSharedWithMe, nil,
      DriveFilePickerFirstLevel::kSharedWithMe);
}
