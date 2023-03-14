// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/scene_util.h"

#import <UIKit/UIKit.h>

#import "base/ios/ios_util.h"
#import "base/strings/sys_string_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Directory containing session files.
const base::FilePath::CharType kSessions[] = FILE_PATH_LITERAL("Sessions");

// Unique identifier used by device that do not support multiple scenes.
NSString* const kSyntheticSessionIdentifier = @"{SyntheticIdentifier}";
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

NSString* SessionIdentifierForScene(UIScene* scene) {
  if (base::ios::IsMultipleScenesSupported()) {
    NSString* identifier = [[scene session] persistentIdentifier];

    DCHECK(identifier.length != 0);
    DCHECK(![kSyntheticSessionIdentifier isEqual:identifier]);
    return identifier;
  }
  return kSyntheticSessionIdentifier;
}
