// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_COLLECTION_H_
#define IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_COLLECTION_H_

#import <Foundation/Foundation.h>

#import <memory>
#import <optional>

#import "ios/chrome/browser/drive_file_picker/ui/drive_file_picker_constants.h"

@protocol SystemIdentity;

// A class that holds information about a collection of Drive files.
// The attributes of this class are immutable.
class DriveFilePickerCollection {
 public:
  // Creates a new root collection.
  static std::unique_ptr<DriveFilePickerCollection> GetRoot(
      id<SystemIdentity> root_identity);

 public:
  // This constructor should only be used by the helper methods.
  DriveFilePickerCollection(
      id<SystemIdentity> identity,
      NSString* title,
      DriveFilePickerCollectionType type,
      NSString* folder_identifier,
      std::optional<DriveFilePickerFirstLevel> first_level);
  ~DriveFilePickerCollection();

  DriveFilePickerCollection(const DriveFilePickerCollection& other) = delete;
  DriveFilePickerCollection& operator=(const DriveFilePickerCollection& other) =
      delete;
  DriveFilePickerCollection(DriveFilePickerCollection&& other) = delete;
  DriveFilePickerCollection& operator=(DriveFilePickerCollection&& other) =
      delete;

  // The identity of the user.
  id<SystemIdentity> GetIdentity() const { return identity_; }
  // The title of the collection.
  NSString* GetTitle() const { return title_; }
  // The type of the collection.
  DriveFilePickerCollectionType GetType() const { return type_; }
  // The identifier of the folder, if this is a folder. Nil otherwise.
  NSString* GetFolderIdentifier() const { return folder_identifier_; }
  // Returns the first level associated with this collection, if any.
  std::optional<DriveFilePickerFirstLevel> GetFirstLevel() const {
    return first_level_;
  }

  // Returns true if the collection is the root collection.
  bool IsRoot() const;
  // Returns true if the collection is a folder collection.
  bool IsFolder() const;
  // Returns true if the collection is the "Shared drives" collection.
  bool IsSharedDrives() const;
  // Returns true if the collection is the "Starred" collection.
  bool IsStarred() const;
  // Returns true if the collection is the "Recent"  collection.
  bool IsRecent() const;
  // Returns true if the collection is the "Shared with me" collection.
  bool IsSharedWithMe() const;

  // Returns true if the collection supports filtering.
  bool SupportsFiltering() const;
  // Returns true if the collection supports sorting.
  bool SupportsSorting() const;

  // Returns the first level collection associated with `identifier`.
  std::unique_ptr<DriveFilePickerCollection> GetFirstLevelCollection(
      NSString* identifier) const;
  // Returns the "My Drive" collection for the same identity.
  std::unique_ptr<DriveFilePickerCollection> GetMyDrive() const;
  // Returns a folder collection for the same identity.
  std::unique_ptr<DriveFilePickerCollection> GetFolder(
      NSString* folder_title,
      NSString* folder_identifier) const;
  // Returns the "Shared drives" collection for the same identity.
  std::unique_ptr<DriveFilePickerCollection> GetSharedDrives() const;
  // Returns the "Starred" collection for the same identity.
  std::unique_ptr<DriveFilePickerCollection> GetStarred() const;
  // Returns the "Recent" collection for the same identity.
  std::unique_ptr<DriveFilePickerCollection> GetRecent() const;
  // Returns the "Shared with me" collection for the same identity.
  std::unique_ptr<DriveFilePickerCollection> GetSharedWithMe() const;

 private:
  // The identity of the user.
  const id<SystemIdentity> identity_;
  // The title of the collection.
  NSString* const title_;
  // The type of the collection.
  const DriveFilePickerCollectionType type_;
  // The identifier of the folder. Should be nil if `type_` is not `kFolder`.
  NSString* const folder_identifier_;
  // First level associated with this collection, if any.
  const std::optional<DriveFilePickerFirstLevel> first_level_;
};

#endif  // IOS_CHROME_BROWSER_DRIVE_FILE_PICKER_COORDINATOR_DRIVE_FILE_PICKER_COLLECTION_H_
