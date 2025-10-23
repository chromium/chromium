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

// Returns all the possible paths to the application's snapshots taken by iOS.
std::vector<base::FilePath> GetSnapshotsPaths();

#endif  // IOS_CHROME_BROWSER_BROWSING_DATA_MODEL_SYSTEM_SNAPSHOTS_CLEANER_H_
