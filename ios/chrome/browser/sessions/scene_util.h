// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_SCENE_UTIL_H_
#define IOS_CHROME_BROWSER_SESSIONS_SCENE_UTIL_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include "base/files/file_path.h"
#include "base/strings/string_piece.h"

// Name of the file storing the list of tabs.
extern const base::FilePath::CharType kSessionFileName[];

// Name of the directory containing the tab snapshots.
extern const base::FilePath::CharType kSnapshotsDirectoryName[];

// Returns the path for the directory relative to the BrowserState `directory`
// that contains the scene session specific storage. All values returned by
// `SessionPathForDirectory` will be below the path returned by this function.
base::FilePath SessionsDirectoryForDirectory(const base::FilePath& directory);

// Returns the path for file or directory named `name` associated with a scene
// identified by `session_identifier` and located relative to the BrowserState
// `directory`.
base::FilePath SessionPathForDirectory(const base::FilePath& directory,
                                       NSString* session_identifier,
                                       base::StringPiece name);

// Migrates the list of tabs and snapshots for a BrowserState's `directory`
// to the given `session_identifier`. The `previous_session_identifier` if
// non-nil is used as a possible previous session for the case of migration
// between devices (in case of backup restoration).
void MigrateSessionStorageForDirectory(const base::FilePath& directory,
                                       NSString* session_identifier,
                                       NSString* previous_session_identifier);

// Returns the identifier to use for the session for `scene`.
NSString* SessionIdentifierForScene(UIScene* scene);

#endif  // IOS_CHROME_BROWSER_SESSIONS_SCENE_UTIL_H_
