// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_CONSTANTS_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_CONSTANTS_H_

#include "base/files/file_path.h"

// Name of the directory containing the tab snapshots.
inline constexpr base::FilePath::StringViewType kSnapshotsDirName =
    FILE_PATH_LITERAL("Snapshots");

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_CONSTANTS_H_
