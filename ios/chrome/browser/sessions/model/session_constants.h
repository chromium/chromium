// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_CONSTANTS_H_
#define IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_CONSTANTS_H_

#include "base/files/file_path.h"

// Name of the directory containing the sessions' storage.
extern const base::FilePath::CharType kSessionRestorationDirname[];

// Name of the session metadata file.
extern const base::FilePath::CharType kSessionMetadataFilename[];

// Name of the file storing the data for a single WebState.
extern const base::FilePath::CharType kWebStateStorageFilename[];

// Name of the file storing the session data for a single WebState.
extern const base::FilePath::CharType kWebStateSessionFilename[];

// Name of the histogram used to record the time spent blocking the main
// thread to save/load the session from storage.
extern const char kSessionHistogramSavingTime[];
extern const char kSessionHistogramLoadingTime[];

#endif  // IOS_CHROME_BROWSER_SESSIONS_MODEL_SESSION_CONSTANTS_H_
