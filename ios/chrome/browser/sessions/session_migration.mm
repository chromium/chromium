// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_migration.h"

#import <Foundation/Foundation.h>

#import "base/apple/foundation_util.h"
#import "base/files/file.h"
#import "base/files/file_enumerator.h"
#import "base/files/file_util.h"
#import "base/logging.h"
#import "base/notreached.h"
#import "base/strings/stringprintf.h"
#import "components/sessions/core/session_id.h"
#import "components/sessions/core/tab_restore_service.h"
#import "components/sessions/ios/ios_restore_live_tab.h"
#import "ios/chrome/browser/sessions/proto/storage.pb.h"
#import "ios/chrome/browser/sessions/session_constants.h"
#import "ios/chrome/browser/sessions/session_internal_util.h"
#import "ios/chrome/browser/sessions/session_ios.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/session/crw_session_user_data.h"
#import "ios/web/public/session/proto/storage.pb.h"

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
//                  metadata.pb
//                  state.pb
//              ...
//          ...

namespace ios::sessions {
namespace {

// Directory containing WebState session for `identifier` relative to `path`.
base::FilePath OptimizedWebStateDirectory(const base::FilePath& path,
                                          web::WebStateID identifier) {
  return path.Append(base::StringPrintf("%08x", identifier.identifier()));
}

// Name of the web session file for `identifier` relative to `path`.
base::FilePath LegacyWebSessionFilename(const base::FilePath& path,
                                        web::WebStateID identifier) {
  return path.Append(base::StringPrintf("%08u", identifier.identifier()));
}

// Writes `session` in optimized format to `path` and returns whether the
// operation was a success.
[[nodiscard]] bool WriteSessionStorageOptimized(const base::FilePath& path,
                                                CRWSessionStorage* session) {
  // Convert `session` to proto.
  web::proto::WebStateStorage storage;
  [session serializeToProto:storage];

  // Write the metadata first.
  if (!WriteProto(path.Append(kWebStateMetadataStorageFilename),
                  storage.metadata())) {
    return false;
  }

  // Clear the metadata from `storage` and save the data. This is how the
  // optimised file format save `data` and `metadata` in two separate files.
  storage.clear_metadata();
  return WriteProto(path.Append(kWebStateStorageFilename), storage);
}

// Loads optimized WebState's state from `path` and converts it to
// CRWSessionStorage*.
[[nodiscard]] CRWSessionStorage* LoadSessionStorageFromOptimized(
    const base::FilePath& path,
    web::WebStateID web_state_id) {
  // Load the data and metadata.
  web::proto::WebStateStorage storage;
  if (!ParseProto(path.Append(kWebStateStorageFilename), storage)) {
    return nil;
  }

  if (!ParseProto(path.Append(kWebStateMetadataStorageFilename),
                  *storage.mutable_metadata())) {
    return nil;
  }

  return [[CRWSessionStorage alloc] initWithProto:storage
                                 uniqueIdentifier:web_state_id
                                 stableIdentifier:[[NSUUID UUID] UUIDString]];
}

// Loads optimized session from `path` and converts it to SessionWindowIOS.
[[nodiscard]] SessionWindowIOS* LoadSessionWindowFromOptimized(
    const base::FilePath& path) {
  // Load the optimized session metadata.
  ios::proto::WebStateListStorage storage;
  if (!ParseProto(path.Append(kSessionMetadataFilename), storage)) {
    return nil;
  }

  // Capture the number of pinned tabs and allocate array to store the tabs.
  const int32_t pinned_items = storage.pinned_item_count();
  NSMutableArray<CRWSessionStorage*>* sessions = [[NSMutableArray alloc] init];

  // Load all the individual tabs' state.
  for (const auto& item : storage.items()) {
    const int32_t index = static_cast<int32_t>(sessions.count);
    const auto ident = web::WebStateID::FromSerializedValue(item.identifier());
    const base::FilePath item_dir = OptimizedWebStateDirectory(path, ident);

    CRWSessionStorage* session =
        LoadSessionStorageFromOptimized(item_dir, ident);
    if (!session) {
      return nil;
    }

    CRWSessionUserData* user_data = nil;
    if (item.has_opener() || index < pinned_items) {
      user_data = [[CRWSessionUserData alloc] init];
      if (item.has_opener()) {
        const ios::proto::OpenerStorage& opener = item.opener();
        [user_data setObject:@(opener.index())
                      forKey:kLegacyWebStateListOpenerIndexKey];
        [user_data setObject:@(opener.navigation_index())
                      forKey:kLegacyWebStateListOpenerNavigationIndexKey];
      }
      if (index < pinned_items) {
        [user_data setObject:@YES forKey:kLegacyWebStateListPinnedStateKey];
      }
    }

    session.userData = user_data;
    session.uniqueIdentifier = ident;
    [sessions addObject:session];
  }

  const NSUInteger selected_index =
      storage.active_index() != -1 ? storage.active_index() : NSNotFound;

  return [[SessionWindowIOS alloc] initWithSessions:sessions
                                      selectedIndex:selected_index];
}

// Deletes data for a legacy session. Ignores errors. Used for cleanup.
void DeleteLegacySession(const base::FilePath& path,
                         const base::FilePath& web_sessions,
                         NSArray<CRWSessionStorage*>* sessions) {
  // Delete the session file, and if empty the session directory.
  std::ignore = DeleteRecursively(path.Append(kLegacySessionFilename));
  if (DirectoryEmpty(path)) {
    std::ignore = DeleteRecursively(path);
  }

  // Delete web sessions file (if possible), and then the directory if empty.
  for (CRWSessionStorage* session in sessions) {
    const base::FilePath web_session_path =
        LegacyWebSessionFilename(web_sessions, session.uniqueIdentifier);

    std::ignore = DeleteRecursively(web_session_path);
  }
  if (DirectoryEmpty(web_sessions)) {
    std::ignore = DeleteRecursively(web_sessions);
  }
}

// Delete data for an optimized session. Ignores errors.
void DeleteOptimizedSession(const base::FilePath& path,
                            const base::FilePath& web_sessions,
                            NSArray<CRWSessionStorage*>* sessions) {
  // Delete the session directory, everything is contained inside.
  std::ignore = DeleteRecursively(path);
}

// Records tabs in `sessions` as recently closed if possible.
void RecordTabsAsRecentlyClosed(::sessions::TabRestoreService* restore_service,
                                NSArray<CRWSessionStorage*>* sessions) {
  if (!restore_service || sessions.count == 0) {
    return;
  }

  int index = 0;
  for (CRWSessionStorage* session in sessions) {
    ::sessions::RestoreIOSLiveTab live_tab(session);
    restore_service->CreateHistoricalTab(&live_tab, index++);
  }
}

// Result of a migration. In case of failure, contains the list of sessions
// that will be discarded (so that they can be reported as recently closed
// tabs, allowing the user to maybe restore them).
struct [[nodiscard]] MigrationResult {
  enum class Status {
    kSkipped,
    kSuccess,
    kFailure,
  };

  const Status status;
  NSArray<CRWSessionStorage*>* const sessions;

  static MigrationResult Skipped() {
    return MigrationResult{.status = Status::kSkipped};
  }

  static MigrationResult Success() {
    return MigrationResult{.status = Status::kSuccess};
  }

  static MigrationResult Failure(NSArray<CRWSessionStorage*>* sessions) {
    return MigrationResult{.status = Status::kFailure, .sessions = sessions};
  }
};

// Migrates session stored in `from` in legacy format to `dest` in optimized
// format. The web sessions files (if present) are stored in `web_sessions`.
// Returns whether the migration status.
MigrationResult MigrateSessionToOptimizedInternal(
    const base::FilePath& from,
    const base::FilePath& dest,
    const base::FilePath& web_sessions) {
  const base::FilePath session_path = from.Append(kLegacySessionFilename);
  if (!FileExists(session_path)) {
    return MigrationResult::Skipped();
  }

  SessionWindowIOS* session = ReadSessionWindow(session_path);
  if (!session) {
    // Can't load session. Can't migrate it, nor record the tabs as closed.
    // Delete the session, so that we don't try to convert it anymore.
    return MigrationResult::Failure(nil);
  }

  for (CRWSessionStorage* item in session.sessions) {
    // Write the item in optimized format.
    const base::FilePath item_path =
        OptimizedWebStateDirectory(dest, item.uniqueIdentifier);
    if (!WriteSessionStorageOptimized(item_path, item)) {
      return MigrationResult::Failure(session.sessions);
    }
  }

  // Migrate the storage for the WebStateList.
  ios::proto::WebStateListStorage storage;
  storage.set_active_index(session.selectedIndex);
  for (CRWSessionStorage* item in session.sessions) {
    ios::proto::WebStateListItemStorage& item_storage = *storage.add_items();
    item_storage.set_identifier(item.uniqueIdentifier.identifier());

    // The legacy format stores some WebStateList metadata in the items.
    // Restore it from there and populate the information in `storage`.
    CRWSessionUserData* user_data = item.userData;
    if (user_data) {
      NSNumber* opener_index = base::apple::ObjCCast<NSNumber>(
          [user_data objectForKey:kLegacyWebStateListOpenerIndexKey]);
      NSNumber* opener_navigation_index = base::apple::ObjCCast<NSNumber>(
          [user_data objectForKey:kLegacyWebStateListOpenerNavigationIndexKey]);

      if (opener_index && opener_navigation_index) {
        ios::proto::OpenerStorage& opener_storage =
            *item_storage.mutable_opener();
        opener_storage.set_index([opener_index intValue]);
        opener_storage.set_navigation_index([opener_navigation_index intValue]);
      }

      NSNumber* is_pinned = base::apple::ObjCCast<NSNumber>(
          [user_data objectForKey:kLegacyWebStateListPinnedStateKey]);
      if (is_pinned && [is_pinned boolValue]) {
        storage.set_pinned_item_count(storage.pinned_item_count() + 1);
      }
    }
  }

  // Write the session metadata.
  const base::FilePath metadata_path = dest.Append(kSessionMetadataFilename);
  if (!WriteProto(metadata_path, storage)) {
    return MigrationResult::Failure(session.sessions);
  }

  // Migrate the web session files if possible.
  for (CRWSessionStorage* item in session.sessions) {
    const base::FilePath web_session_from_path =
        LegacyWebSessionFilename(web_sessions, item.uniqueIdentifier);
    if (!FileExists(web_session_from_path)) {
      continue;
    }

    // Rename the web session file (failure is okay, the code can load
    // the session even if the file is missing).
    const base::FilePath web_session_dest_path =
        OptimizedWebStateDirectory(dest, item.uniqueIdentifier)
            .Append(kWebStateSessionFilename);
    std::ignore = RenameFile(web_session_from_path, web_session_dest_path);
  }

  return MigrationResult::Success();
}

// Migrates session stored in `from` in optimized format to `dest` in legacy
// format. The web sessions files (if present) are stored in `web_sessions`.
// Returns whether the migration status.
MigrationResult MigrateSessionToLegacyInternal(
    const base::FilePath& from,
    const base::FilePath& dest,
    const base::FilePath& web_sessions) {
  const base::FilePath metadata_path = from.Append(kSessionMetadataFilename);
  if (!FileExists(metadata_path)) {
    return MigrationResult::Skipped();
  }

  // Load the optimized session and convert it in memory to the legacy format.
  SessionWindowIOS* session = LoadSessionWindowFromOptimized(from);
  if (!session) {
    // Can't load session. Can't migrate it, nor record the tabs as closed.
    // Delete the session, so that we don't try to convert it anymore.
    return MigrationResult::Failure(nil);
  }

  // Write the legacy session to destination.
  if (!WriteSessionWindow(dest.Append(kLegacySessionFilename), session)) {
    return MigrationResult::Failure(session.sessions);
  }

  // Migrate the web session files if possible.
  for (CRWSessionStorage* item in session.sessions) {
    const base::FilePath web_session_from_path =
        OptimizedWebStateDirectory(from, item.uniqueIdentifier)
            .Append(kWebStateSessionFilename);
    if (!FileExists(web_session_from_path)) {
      continue;
    }

    // Rename the web session file (failure is okay, the code can load
    // the session even if the file is missing).
    const base::FilePath web_session_dest_path =
        LegacyWebSessionFilename(web_sessions, item.uniqueIdentifier);
    std::ignore = RenameFile(web_session_from_path, web_session_dest_path);
  }

  return MigrationResult::Success();
}

// Migrates session stored in `from` in legacy format to `dest` in optimized
// format and performs cleanup.
void MigrateSessionToOptimizedWithCleanup(
    const base::FilePath& from,
    const base::FilePath& dest,
    const base::FilePath& web_sessions,
    ::sessions::TabRestoreService* restore_service) {
  const MigrationResult result =
      MigrateSessionToOptimizedInternal(from, dest, web_sessions);

  switch (result.status) {
    case MigrationResult::Status::kSkipped:
      if (DirectoryEmpty(from)) {
        std::ignore = DeleteRecursively(from);
      }
      if (DirectoryEmpty(web_sessions)) {
        std::ignore = DeleteRecursively(web_sessions);
      }
      break;

    case MigrationResult::Status::kFailure:
      // In case of failure, the code will fall back to an empty session.
      // If any tabs were loaded, then record them as recently closed.
      RecordTabsAsRecentlyClosed(restore_service, result.sessions);

      // Delete any content that may have been written in the optimized session.
      DeleteOptimizedSession(dest, web_sessions, result.sessions);

      // Fall through to the cleanup of the legacy sessions.
      [[fallthrough]];

    case MigrationResult::Status::kSuccess:
      // Delete the legacy sessions files (note that the legacy sessions
      // directory is used by other features, so int must be kept if not
      // empty) and the web sessions cache (if empty, since it is shared
      // by all sessions).
      DeleteLegacySession(from, web_sessions, result.sessions);
      break;
  }

  const base::FilePath parent_dir = from.DirName();
  if (DirectoryEmpty(parent_dir)) {
    std::ignore = DeleteRecursively(parent_dir);
  }
}

// Migrates session stored in `from` in optimized format to `dest` in legacy
// format and performs cleanup.
void MigrateSessionToLegacyWithCleanup(
    const base::FilePath& from,
    const base::FilePath& dest,
    const base::FilePath& web_sessions,
    ::sessions::TabRestoreService* restore_service) {
  const MigrationResult result =
      MigrateSessionToLegacyInternal(from, dest, web_sessions);

  switch (result.status) {
    case MigrationResult::Status::kSkipped:
      if (DirectoryEmpty(from)) {
        std::ignore = DeleteRecursively(from);
      }
      break;

    case MigrationResult::Status::kFailure:
      // In case of failure, the code will fall back to an empty session.
      // If any tabs were loaded, then record them as recently closed.
      RecordTabsAsRecentlyClosed(restore_service, result.sessions);

      // Delete any content that may have been written in the optimized session.
      DeleteLegacySession(dest, web_sessions, result.sessions);

      // Fall through to the cleanup of the optimized sessions.
      [[fallthrough]];

    case MigrationResult::Status::kSuccess:
      // Delete the optimized session storage. Everything is stored in a
      // single directory, and it is not reused by other features, so it
      // can be deleted.
      DeleteOptimizedSession(from, web_sessions, result.sessions);
      break;
  }

  const base::FilePath parent_dir = from.DirName();
  if (DirectoryEmpty(parent_dir)) {
    std::ignore = DeleteRecursively(parent_dir);
  }
}

}  // namespace

void MigrateNamedSessionToOptimized(
    const base::FilePath& path,
    const std::string& name,
    ::sessions::TabRestoreService* restore_service) {
  MigrateSessionToOptimizedWithCleanup(
      path.Append(kLegacySessionsDirname).Append(name),
      path.Append(kSessionRestorationDirname).Append(name),
      path.Append(kLegacyWebSessionsDirname), restore_service);
}

void MigrateNamedSessionToLegacy(
    const base::FilePath& path,
    const std::string& name,
    ::sessions::TabRestoreService* restore_service) {
  MigrateSessionToLegacyWithCleanup(
      path.Append(kSessionRestorationDirname).Append(name),
      path.Append(kLegacySessionsDirname).Append(name),
      path.Append(kLegacyWebSessionsDirname), restore_service);
}

}  // namespace ios::sessions
