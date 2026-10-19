// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/snapshot_storage_util.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/snapshots/model/model_swift.h"

id<SnapshotStorage> CreateSnapshotStorage(const base::FilePath& storage_path) {
  return [[SnapshotStorageImpl alloc]
      initWithStorageDirectoryUrl:base::apple::FilePathToNSURL(storage_path)];
}
