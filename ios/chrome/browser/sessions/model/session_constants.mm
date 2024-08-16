// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/session_constants.h"

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

NSString* const kLegacyWebStateListPinnedStateKey = @"PinnedState";

NSString* const kLegacyWebStateListOpenerIndexKey = @"OpenerIndex";

NSString* const kLegacyWebStateListOpenerNavigationIndexKey =
    @"OpenerNavigationIndex";

const char kSessionStorageFormatPref[] = "ios.session.storage.format";

const char kSessionStorageMigrationStatusPref[] =
    "ios.session.storage.migration-status";

const char kSessionStorageMigrationStartedTimePref[] =
    "ios.session.storage.migration-start-time";

const char kSessionHistogramSavingTime[] =
    "Session.WebStates.SavingTimeOnMainThread";

const char kSessionHistogramLoadingTime[] =
    "Session.WebStates.LoadingTimeOnMainThread";

const char kSessionHistogramStorageFormat[] = "Session.WebStates.StorageFormat";

const char kSessionHistogramStorageMigrationStatus[] =
    "Session.WebStates.StorageMigrationStatus";

const char kSessionHistogramStorageMigrationTiming[] =
    "Session.WebStates.StorageMigrationDuration";
