// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/scene_util.h"

#import <UIKit/UIKit.h>

#import "base/ios/ios_util.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Enumeration used to represent the type of session migration that took place.
// The enumerator must not be removed or their order changed as the histogram
// IOS.SessionMigration is recording them.
enum class SessionMigration {
  kNoMigration = 0,
  kMigrationFailed = 1,
  kMigrationNoSessionToMigrate = 2,
  kMigrationPreMWToMultiScenes = 3,
  kMigrationPreMWToSingleScene = 4,
  kMigrationSingleSceneToMultiScenes = 5,
  kMigrationMultiScenesToSingleScene = 6,
  kMaxValue = kMigrationMultiScenesToSingleScene,
};

// Directory containing session files.
const base::FilePath::CharType kSessions[] = FILE_PATH_LITERAL("Sessions");

// Unique identifier used by device that do not support multiple scenes.
NSString* const kSyntheticSessionIdentifier = @"{SyntheticIdentifier}";

// Converts a base::FilePath to a NSString*.
NSString* PathToNSString(const base::FilePath& file_path) {
  return base::SysUTF8ToNSString(file_path.AsUTF8Unsafe());
}

SessionMigration MigrateSessionStorageForDirectoryImpl(
    const base::FilePath& directory,
    NSString* session_identifier,
    NSArray<NSString*>* previous_identifiers) {
  DCHECK(session_identifier.length != 0);

  NSFileManager* file_manager = [NSFileManager defaultManager];

  const std::string session = base::SysNSStringToUTF8(session_identifier);
  NSString* session_directory =
      PathToNSString(directory.Append(kSessions).Append(session));

  // If the new directory already exists, then there is no need to perform
  // any migration.
  if ([file_manager fileExistsAtPath:session_directory])
    return SessionMigration::kNoMigration;

  // List of files to use to identify the previous session directory (and also
  // to migrate to the new path).
  const base::FilePath::CharType* kCandidateNames[] = {kSessionFileName,
                                                       kSnapshotsDirectoryName};

  // Try to identify the previous session directory. This is done by iterating
  // over the possible previous session identifier, and looking for the files
  // that are known to store the list of tabs or their snapshots. As soon as
  // one of the file is found, consider the identifier found.
  NSString* previous_session_identifier = nil;
  for (NSString* identifier in previous_identifiers) {
    for (const base::FilePath::CharType* name : kCandidateNames) {
      NSString* path =
          PathToNSString(SessionPathForDirectory(directory, identifier, name));
      if ([file_manager fileExistsAtPath:path]) {
        previous_session_identifier = identifier;
        break;
      }
    }
    if (previous_session_identifier)
      break;
  }

  // Create the new directory used to store the session, aborting if the
  // creation failed (since the migration won't be possible then).
  NSError* error = nil;
  if (![file_manager createDirectoryAtPath:session_directory
               withIntermediateDirectories:YES
                                attributes:nil
                                     error:&error]) {
    return SessionMigration::kMigrationFailed;
  }

  // If no identifier was found, then abort the migration. This is done after
  // creating the new directory to avoid unnecessarily trying to perform the
  // migration again on next startup.
  if (!previous_session_identifier)
    return SessionMigration::kMigrationNoSessionToMigrate;

  // Migrate all the individual files (only attempt the migration if the file
  // exist). Errors are logged but do not abort the migration.
  for (const base::FilePath::CharType* name : kCandidateNames) {
    NSString* path = PathToNSString(
        SessionPathForDirectory(directory, previous_session_identifier, name));
    if (![file_manager fileExistsAtPath:path])
      continue;

    NSString* destination = PathToNSString(
        SessionPathForDirectory(directory, session_identifier, name));
    if (![file_manager moveItemAtPath:path toPath:destination error:&error]) {
      return SessionMigration::kMigrationFailed;
    }
  }

  // If the previous session identifier was not empty, then it was located in
  // the Sessions sub-directory of the BrowserState directory. As its content
  // has been migrated, the directory itself should be empty, so delete it.
  if (previous_session_identifier.length != 0) {
    NSString* previous_session_directory =
        PathToNSString(directory.Append(kSessions).Append(
            base::SysNSStringToUTF8(previous_session_identifier)));

    if (![file_manager removeItemAtPath:previous_session_directory
                                  error:&error]) {
      return SessionMigration::kMigrationFailed;
    }

    return [previous_session_identifier isEqual:kSyntheticSessionIdentifier]
               ? SessionMigration::kMigrationSingleSceneToMultiScenes
               : SessionMigration::kMigrationMultiScenesToSingleScene;
  }

  return base::ios::IsMultipleScenesSupported()
             ? SessionMigration::kMigrationPreMWToMultiScenes
             : SessionMigration::kMigrationPreMWToSingleScene;
}

}

const base::FilePath::CharType kSessionFileName[] =
    FILE_PATH_LITERAL("session.plist");

const base::FilePath::CharType kSnapshotsDirectoryName[] =
    FILE_PATH_LITERAL("Snapshots");

base::FilePath SessionsDirectoryForDirectory(const base::FilePath& directory) {
  return directory.Append(kSessions);
}

base::FilePath SessionPathForDirectory(const base::FilePath& directory,
                                       NSString* session_identifier,
                                       base::StringPiece name) {
  // This is to support migration from old version of Chrome or old devices
  // that were not using multi-window API. Remove once all user have migrated
  // and there is no need to restore their old sessions.
  if (!session_identifier.length)
    return directory.Append(name);

  const std::string session = base::SysNSStringToUTF8(session_identifier);
  return directory.Append(kSessions).Append(session).Append(name);
}

void MigrateSessionStorageForDirectory(const base::FilePath& directory,
                                       NSString* session_identifier,
                                       NSString* previous_session_identifier) {
  DCHECK(session_identifier.length != 0);
  NSMutableArray<NSString*>* previous_identifiers = [NSMutableArray array];

  // Support migrating from Chrome M-{87-89} on a device that does not support
  // multiple scenes (where the session identifier was not constant). See
  // crbug.com/1165798 for information on why this was a problem.
  if (previous_session_identifier.length != 0)
    [previous_identifiers addObject:previous_session_identifier];

  // Support migrating from a device not supporting multiple scenes to one that
  // does (e.g. migrating after upgrading an iPad to iOS 13+, restoring an
  // iPhone backup on an iPad, ...).
  if (base::ios::IsMultipleScenesSupported())
    [previous_identifiers addObject:kSyntheticSessionIdentifier];

  // Support migrating from Chrome pre M-87 which did not support multi-window
  // API and thus did not use session identifier in the path to the files used
  // to store the list of tabs or the snapshots.
  [previous_identifiers addObject:@""];

  const SessionMigration session_migration_status =
      MigrateSessionStorageForDirectoryImpl(directory, session_identifier,
                                            previous_identifiers);

  base::UmaHistogramEnumeration("IOS.SessionMigration",
                                session_migration_status);
}

NSString* SessionIdentifierForScene(UIScene* scene) {
  if (base::ios::IsMultipleScenesSupported()) {
    NSString* identifier = [[scene session] persistentIdentifier];

    DCHECK(identifier.length != 0);
    DCHECK(![kSyntheticSessionIdentifier isEqual:identifier]);
    return identifier;
  }
  return kSyntheticSessionIdentifier;
}
