// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_SYSTEM_SNAPSHOTS_CLEANER_H_
#define IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_SYSTEM_SNAPSHOTS_CLEANER_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"

// Clears the application snapshots taken by iOS and invoke `callback` when
// the deletion has completed (asynchronously).
void ClearIOSSnapshots(base::OnceClosure callback);

// Adds to `snapshotsPaths` all the possible paths to the application's
// snapshots taken by iOS.
void GetSnapshotsPaths(std::vector<base::FilePath>* snapshotsPaths);

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_SYSTEM_SNAPSHOTS_CLEANER_H_
