// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_migration.h"

#import <Foundation/Foundation.h>

#import <optional>

#import "base/apple/foundation_util.h"
#import "base/files/file.h"
#import "base/files/file_enumerator.h"
#import "base/files/file_util.h"
#import "base/logging.h"
#import "base/notreached.h"
#import "base/strings/stringprintf.h"
#import "ios/chrome/browser/sessions/proto/storage.pb.h"
#import "ios/chrome/browser/sessions/session_constants.h"
#import "ios/chrome/browser/sessions/session_internal_util.h"
#import "ios/chrome/browser/sessions/session_ios.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/session/crw_session_user_data.h"
#import "ios/web/public/session/proto/storage.pb.h"
#import "ios/web/public/web_state_id.h"

// This file provides utilities to migrate storage for sessions from the
// legacy to the optimized format (or reciprocally).
//
// The functions performs the conversion without using the code from the
// SessionServiceIOS or SessionRestorationServiceImpl as those services
// are designed to load/save sessions for a Browser and instantiate the
// WebStates, but we want to be able to convert the storage without the
// creation of all individual objects.
//
// For this reason, those migration functions are separate implementation
// but they heavily depends on the file layout used by those services. So
// any change to the services should be reflected here.

// The legacy storage is the following:
//  ${BrowserStatePath}/
//      Sessions/
//          ${SessionID}/
//              session.plist
//          ...
//      Web_Sessions/
//          ${WebStateID}
//          ...

// The optimized storage is the following:
//  ${BrowserStatePath}
//      SessionStorage/
//          ${SessionID}/
//              session_metadata.pb
//              ${WebStateID}/
//                  data.pb
//                  state.pb
//              ...
//          ...

namespace ios::sessions {
namespace {

// Helper class used to simplify the conversion of session between legacy
// and optimised format.
class OptimizedSession {
 public:
  // Creates an instance from `legacy_session` in legacy format.
  static std::optional<OptimizedSession> FromLegacy(
      SessionWindowIOS* legacy_session);

  // Creates an instance loading a session in optimized format from
  // `session_dir`.
  static std::optional<OptimizedSession> FromPath(
      const base::FilePath& session_dir);

  // Converts the session to legacy format.
  SessionWindowIOS* ToLegacy() const;

  // Saves the session in optimised format at `session_dir`. The native
  // WKWebView session data can be found in `web_sessions`.
  bool SaveTo(const base::FilePath& session_dir,
              const base::FilePath& web_sessions) const;

 private:
  OptimizedSession(ios::proto::WebStateListStorage metadata_storage,
                   std::vector<web::proto::WebStateStorage> storage);

  explicit OptimizedSession(SessionWindowIOS* legacy_session);

  // Helper adding an item to the current object from its legacy
  // representation in `item`.
  void AddItem(CRWSessionStorage* item);

  ios::proto::WebStateListStorage metadata_storage_;
  std::vector<web::proto::WebStateStorage> storage_;
};

// static
std::optional<OptimizedSession> OptimizedSession::FromLegacy(
    SessionWindowIOS* legacy_session) {
  return OptimizedSession(legacy_session);
}

// static
std::optional<OptimizedSession> OptimizedSession::FromPath(
    const base::FilePath& session_dir) {
  const base::FilePath session_path =
      session_dir.Append(kSessionMetadataFilename);

  ios::proto::WebStateListStorage metadata_storage;
  if (!ParseProto(session_path, metadata_storage)) {
    return std::nullopt;
  }

  const int count = metadata_storage.items_size();
  std::vector<web::proto::WebStateStorage> storage;
  storage.reserve(count);

  for (int index = 0; index < count; ++index) {
    const ios::proto::WebStateListItemStorage& item_storage =
        metadata_storage.items(index);

    const base::FilePath item_dir = session_dir.Append(
        base::StringPrintf("%08x", item_storage.identifier()));

    // While developing the optimised session storage, at some point, the
    // metadata for WebStates were saved in individual files. As all those
    // metadata files had to be loaded on startup, this resulted in many
    // file loads for users with a large number of tabs. This code is here
    // to convert those sessions to the new storage.
    //
    // Since saving in many individual files was never released to stable,
    // nor enabled via finch, the only users that manually enabled it via
    // chrome://flags may have the data in that state.
    //
    // Thus there is no need to keep this code for many releases (as the
    // feature was not yet supported when enabled). This workaround can
    // be removed as soon as M-123.
    //
    // TODO(crbug.com/1504753): cleanup when no longer required.
    if (!item_storage.has_metadata()) {
      const base::FilePath item_metadata_path =
          item_dir.Append(kWebStateMetadataStorageFilename);

      google::protobuf::RepeatedPtrField<ios::proto::WebStateListItemStorage>&
          repeated_field = *metadata_storage.mutable_items();

      web::proto::WebStateMetadataStorage& item_metadata =
          *(repeated_field[index].mutable_metadata());

      if (!ParseProto(item_metadata_path, item_metadata)) {
        return std::nullopt;
      }
    }

    const base::FilePath item_path = item_dir.Append(kWebStateStorageFilename);
    if (!ParseProto(item_path, storage.emplace_back())) {
      return std::nullopt;
    }
  }

  return OptimizedSession(std::move(metadata_storage), std::move(storage));
}

SessionWindowIOS* OptimizedSession::ToLegacy() const {
  DCHECK_EQ(metadata_storage_.items_size(), static_cast<int>(storage_.size()));
  const int count = metadata_storage_.items_size();
  const int pinned_count = metadata_storage_.pinned_item_count();

  NSMutableArray<CRWSessionStorage*>* items = [[NSMutableArray alloc] init];
  for (int index = 0; index < count; ++index) {
    const ios::proto::WebStateListItemStorage& item_storage =
        metadata_storage_.items(index);

    web::proto::WebStateStorage item_data_storage = storage_[index];
    *item_data_storage.mutable_metadata() = item_storage.metadata();

    const web::WebStateID identifier =
        web::WebStateID::FromSerializedValue(item_storage.identifier());

    CRWSessionStorage* item =
        [[CRWSessionStorage alloc] initWithProto:item_data_storage
                                uniqueIdentifier:identifier
                                stableIdentifier:[[NSUUID UUID] UUIDString]];

    if (index < pinned_count || item_storage.has_opener()) {
      CRWSessionUserData* user_data = [[CRWSessionUserData alloc] init];

      if (index < pinned_count) {
        [user_data setObject:@YES forKey:kLegacyWebStateListPinnedStateKey];
      }

      if (item_storage.has_opener()) {
        const ios::proto::OpenerStorage& opener_storage = item_storage.opener();
        [user_data setObject:@(opener_storage.index())
                      forKey:kLegacyWebStateListOpenerIndexKey];
        [user_data setObject:@(opener_storage.navigation_index())
                      forKey:kLegacyWebStateListOpenerNavigationIndexKey];
      }

      item.userData = user_data;
    }

    [items addObject:item];
  }

  NSUInteger selected_index = NSNotFound;
  const int active_index = metadata_storage_.active_index();
  if (0 <= active_index && active_index < count) {
    selected_index = static_cast<NSUInteger>(active_index);
  }

  return [[SessionWindowIOS alloc] initWithSessions:items
                                      selectedIndex:selected_index];
}

bool OptimizedSession::SaveTo(const base::FilePath& session_dir,
                              const base::FilePath& web_sessions) const {
  DCHECK_EQ(metadata_storage_.items_size(), static_cast<int>(storage_.size()));
  const int count = metadata_storage_.items_size();

  // First write the individual WebState's data.
  for (int index = 0; index < count; ++index) {
    const ios::proto::WebStateListItemStorage& item_storage =
        metadata_storage_.items(index);

    const base::FilePath item_dir = session_dir.Append(
        base::StringPrintf("%08x", item_storage.identifier()));

    const base::FilePath item_path = item_dir.Append(kWebStateStorageFilename);

    // Save the WebState data.
    if (!WriteProto(item_path, storage_[index])) {
      return false;
    }

    const base::FilePath item_native_data_path = web_sessions.Append(
        base::StringPrintf("%08u", item_storage.identifier()));

    // Copy the WebState WKWebView native data if it exists. It is okay if
    // the copy fails, since loading the sessions accepts their absence.
    if (FileExists(item_native_data_path)) {
      std::ignore = ios::sessions::CopyFile(
          item_native_data_path, item_dir.Append(kWebStateSessionFilename));
    }
  }

  const base::FilePath session_path =
      session_dir.Append(kSessionMetadataFilename);

  // Save the session metadata.
  if (!WriteProto(session_path, metadata_storage_)) {
    return false;
  }

  return true;
}

OptimizedSession::OptimizedSession(
    ios::proto::WebStateListStorage metadata_storage,
    std::vector<web::proto::WebStateStorage> storage)
    : metadata_storage_(std::move(metadata_storage)),
      storage_(std::move(storage)) {}

OptimizedSession::OptimizedSession(SessionWindowIOS* legacy_session) {
  metadata_storage_.set_active_index(legacy_session.selectedIndex);
  for (CRWSessionStorage* legacy_item in legacy_session.sessions) {
    AddItem(legacy_item);
  }
}

void OptimizedSession::AddItem(CRWSessionStorage* legacy_item) {
  ios::proto::WebStateListItemStorage& item = *metadata_storage_.add_items();
  item.set_identifier(legacy_item.uniqueIdentifier.identifier());

  // Serialize the item to protobuf message format, and move the metadata
  // to the WebStateListStorage (since is is where the optimised format
  // stores the WebState's metadata).
  [legacy_item serializeToProto:storage_.emplace_back()];
  DCHECK(storage_.back().has_metadata());

  std::unique_ptr<web::proto::WebStateMetadataStorage> item_metadata(
      storage_.back().release_metadata());
  DCHECK(!storage_.back().has_metadata());

  item_metadata->Swap(item.mutable_metadata());
  DCHECK(item.has_metadata());

  // The legacy format stores some WebStateList metadata in `item`.
  CRWSessionUserData* user_data = legacy_item.userData;
  if (user_data) {
    NSNumber* opener_index = base::apple::ObjCCast<NSNumber>(
        [user_data objectForKey:kLegacyWebStateListOpenerIndexKey]);
    NSNumber* opener_navigation_index = base::apple::ObjCCast<NSNumber>(
        [user_data objectForKey:kLegacyWebStateListOpenerNavigationIndexKey]);

    if (opener_index && opener_navigation_index) {
      ios::proto::OpenerStorage& opener_storage = *item.mutable_opener();
      opener_storage.set_index([opener_index intValue]);
      opener_storage.set_navigation_index([opener_navigation_index intValue]);
    }

    NSNumber* is_pinned = base::apple::ObjCCast<NSNumber>(
        [user_data objectForKey:kLegacyWebStateListPinnedStateKey]);
    if (is_pinned && [is_pinned boolValue]) {
      metadata_storage_.set_pinned_item_count(
          metadata_storage_.pinned_item_count() + 1);
    }
  }

  // Check the class invariants.
  DCHECK_EQ(metadata_storage_.items_size(), static_cast<int>(storage_.size()));
  DCHECK_LE(metadata_storage_.pinned_item_count(),
            metadata_storage_.items_size());
}

// Migrates session stored in `from` in legacy format to `dest` in optimized
// format. The web sessions files (if present) are stored in `web_sessions`.
// Returns whether the migration status.
MigrationStatus MigrateSessionToOptimizedInternal(
    const base::FilePath& from,
    const base::FilePath& dest,
    const base::FilePath& web_sessions) {
  const base::FilePath legacy_path = from.Append(kLegacySessionFilename);
  if (!FileExists(legacy_path)) {
    return MigrationStatus::kSuccess;
  }

  SessionWindowIOS* legacy = ReadSessionWindow(legacy_path);
  if (!legacy) {
    return MigrationStatus::kFailure;
  }

  std::optional<OptimizedSession> optimized =
      OptimizedSession::FromLegacy(legacy);

  if (!optimized || !optimized->SaveTo(dest, web_sessions)) {
    return MigrationStatus::kFailure;
  }

  return MigrationStatus::kSuccess;
}

// Migrates session stored in `from` in optimized format to `dest` in legacy
// format. The web sessions files (if present) are stored in `web_sessions`.
// Returns whether the migration status.
MigrationStatus MigrateSessionToLegacyInternal(
    const base::FilePath& from,
    const base::FilePath& dest,
    const base::FilePath& web_sessions) {
  const base::FilePath metadata_path = from.Append(kSessionMetadataFilename);
  if (!FileExists(metadata_path)) {
    return MigrationStatus::kSuccess;
  }

  std::optional<OptimizedSession> optimized = OptimizedSession::FromPath(from);
  if (!optimized) {
    return MigrationStatus::kFailure;
  }

  SessionWindowIOS* legacy = optimized->ToLegacy();
  DCHECK(legacy);

  // Write the legacy session to destination.
  if (!WriteSessionWindow(dest.Append(kLegacySessionFilename), legacy)) {
    return MigrationStatus::kFailure;
  }

  // Migrate the web session files if possible.
  for (CRWSessionStorage* item in legacy.sessions) {
    const base::FilePath item_dir = from.Append(
        base::StringPrintf("%08x", item.uniqueIdentifier.identifier()));

    const base::FilePath item_native_data_path =
        item_dir.Append(kWebStateSessionFilename);

    // Copy the WebState WKWebView native data if it exists. It is okay if
    // the copy fails, since loading the sessions accepts their absence.
    if (FileExists(item_native_data_path)) {
      std::ignore = ios::sessions::CopyFile(
          item_native_data_path,
          web_sessions.Append(
              base::StringPrintf("%08u", item.uniqueIdentifier.identifier())));
    }
  }

  return MigrationStatus::kSuccess;
}

// Helper for MigrateSessionsInPathsToOptimized(...) that migrate the data
// but performs no cleanup. It stops at the first failure.
MigrationStatus MigrateSessionsInPathsToOptimizedNoCleanup(
    const std::vector<base::FilePath>& paths) {
  for (const base::FilePath& path : paths) {
    const base::FilePath from_dir = path.Append(kLegacySessionsDirname);
    const base::FilePath dest_dir = path.Append(kSessionRestorationDirname);
    const base::FilePath sessions = path.Append(kLegacyWebSessionsDirname);

    const int file_types = base::FileEnumerator::DIRECTORIES;
    base::FileEnumerator iter(from_dir, false, file_types);
    for (base::FilePath name = iter.Next(); !name.empty(); name = iter.Next()) {
      const base::FilePath basename = name.BaseName();
      const MigrationStatus result = MigrateSessionToOptimizedInternal(
          from_dir.Append(basename), dest_dir.Append(basename), sessions);

      if (result != MigrationStatus::kSuccess) {
        return MigrationStatus::kFailure;
      }
    }
  }

  return MigrationStatus::kSuccess;
}

// Helper for MigrateSessionsInPathsToLegacy(...) that migrate the data
// but performs no cleanup. It stops at the first failure.
MigrationStatus MigrateSessionsInPathsToLegacyNoCleanup(
    const std::vector<base::FilePath>& paths) {
  for (const base::FilePath& path : paths) {
    const base::FilePath from_dir = path.Append(kSessionRestorationDirname);
    const base::FilePath dest_dir = path.Append(kLegacySessionsDirname);
    const base::FilePath sessions = path.Append(kLegacyWebSessionsDirname);

    const int file_types = base::FileEnumerator::DIRECTORIES;
    base::FileEnumerator iter(from_dir, false, file_types);
    for (base::FilePath name = iter.Next(); !name.empty(); name = iter.Next()) {
      const base::FilePath basename = name.BaseName();
      const MigrationStatus result = MigrateSessionToLegacyInternal(
          from_dir.Append(basename), dest_dir.Append(basename), sessions);

      if (result != MigrationStatus::kSuccess) {
        return MigrationStatus::kFailure;
      }
    }
  }

  return MigrationStatus::kSuccess;
}

// Deletes optimized session directories in `paths`.
void DeleteOptimizedSessions(const std::vector<base::FilePath>& paths) {
  for (const base::FilePath& path : paths) {
    const base::FilePath optimized = path.Append(kSessionRestorationDirname);
    std::ignore = DeleteRecursively(optimized);
  }
}

// Deletes legacy session directories in `paths` taking care of leaving
// any unrelated content unaffected.
void DeleteLegacySessions(const std::vector<base::FilePath>& paths) {
  for (const base::FilePath& path : paths) {
    const base::FilePath legacy = path.Append(kLegacySessionsDirname);

    const int file_types = base::FileEnumerator::DIRECTORIES;
    base::FileEnumerator iter(legacy, false, file_types);
    for (base::FilePath name = iter.Next(); !name.empty(); name = iter.Next()) {
      std::ignore = DeleteRecursively(name);
    }
    if (ios::sessions::DirectoryEmpty(legacy)) {
      std::ignore = DeleteRecursively(legacy);
    }

    const base::FilePath sessions = path.Append(kLegacyWebSessionsDirname);
    std::ignore = DeleteRecursively(sessions);
  }
}

}  // namespace

MigrationStatus MigrateSessionsInPathsToOptimized(
    const std::vector<base::FilePath>& paths) {
  // Try to perform the migration, stopping at the first failure.
  const MigrationStatus status =
      MigrateSessionsInPathsToOptimizedNoCleanup(paths);

  // Cleanup after the migration by deleting either the partially migrated
  // data (in case of failure) or original data (in case of success).
  switch (status) {
    case MigrationStatus::kSuccess:
      // The data has been successfully migrated to optimized storage,
      // delete the legacy storage (including the cache of WKWebView
      // native session data).
      DeleteLegacySessions(paths);
      break;

    case MigrationStatus::kFailure:
      // The migration to optimized format failed, delete any data that
      // may have been written in the optimised storage directory.
      DeleteOptimizedSessions(paths);
      break;
  }

  return status;
}

MigrationStatus MigrateSessionsInPathsToLegacy(
    const std::vector<base::FilePath>& paths) {
  // Try to perform the migration, stopping at the first failure.
  const MigrationStatus status = MigrateSessionsInPathsToLegacyNoCleanup(paths);

  // Cleanup after the migration by deleting either the partially migrated
  // data (in case of failure) or original data (in case of success).
  switch (status) {
    case MigrationStatus::kSuccess:
      // The data has been successfully migrated to legacy storage,
      // delete the optimized storage.
      DeleteOptimizedSessions(paths);
      break;

    case MigrationStatus::kFailure:
      // The migration to legacy format failed, delete any data that
      // may have been written in the legacy storage directory. Also
      // delete the cache of WKWebView native session data.
      DeleteLegacySessions(paths);
      break;
  }

  return status;
}

}  // namespace ios::sessions
