// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_STORAGE_UTIL_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_STORAGE_UTIL_H_

@protocol SnapshotStorage;

namespace base {
class FilePath;
}

// Creates an instance of SnapshotStorage with `storage_path`.
id<SnapshotStorage> CreateSnapshotStorage(const base::FilePath& storage_path);

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_STORAGE_UTIL_H_
