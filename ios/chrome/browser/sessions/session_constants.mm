// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_constants.h"

const base::FilePath::CharType kLegacySessionsDirname[] =
    FILE_PATH_LITERAL("Sessions");

const base::FilePath::CharType kLegacyWebSessionsDirname[] =
    FILE_PATH_LITERAL("Web_Sessions");

const base::FilePath::CharType kLegacySessionFilename[] =
    FILE_PATH_LITERAL("session.plist");

const base::FilePath::CharType kSessionRestorationDirname[] =
    FILE_PATH_LITERAL("SessionStorage");

const base::FilePath::CharType kSessionMetadataFilename[] =
    FILE_PATH_LITERAL("session_metadata.pb");

const base::FilePath::CharType kWebStateStorageFilename[] =
    FILE_PATH_LITERAL("data.pb");

const base::FilePath::CharType kWebStateSessionFilename[] =
    FILE_PATH_LITERAL("state.pb");

const base::FilePath::CharType kWebStateMetadataStorageFilename[] =
    FILE_PATH_LITERAL("metadata.pb");

NSString* const kLegacyWebStateListPinnedStateKey = @"PinnedState";

NSString* const kLegacyWebStateListOpenerIndexKey = @"OpenerIndex";

NSString* const kLegacyWebStateListOpenerNavigationIndexKey =
    @"OpenerNavigationIndex";
