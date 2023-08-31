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

NSString* const kLegacyWebStateListPinnedStateKey = @"PinnedState";

NSString* const kLegacyWebStateListOpenerIndexKey = @"OpenerIndex";

NSString* const kLegacyWebStateListOpenerNavigationIndexKey =
    @"OpenerNavigationIndex";
