// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/browsing_data/model/system_snapshots_cleaner.h"

#import <UIKit/UIKit.h>

#import "base/apple/foundation_util.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/location.h"
#import "base/path_service.h"
#import "base/strings/stringprintf.h"
#import "base/task/thread_pool.h"

namespace {
const char* kOrientationDescriptions[] = {
    "LandscapeLeft",
    "LandscapeRight",
    "Portrait",
    "PortraitUpsideDown",
};

// Delete all files in `paths`.
void DeleteAllFiles(std::vector<base::FilePath> paths) {
  for (const auto& path : paths) {
    base::DeleteFile(path);
  }
}
}  // namespace

void ClearIOSSnapshots(base::OnceClosure callback) {
  // Generates a list containing all the possible snapshot paths because the
  // list of snapshots stored on the device can't be obtained programmatically.
  std::vector<base::FilePath> snapshots_paths;
  GetSnapshotsPaths(&snapshots_paths);
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&DeleteAllFiles, std::move(snapshots_paths)),
      std::move(callback));
}

void GetSnapshotsPaths(std::vector<base::FilePath>* snapshots_paths) {
  DCHECK(snapshots_paths);
  base::FilePath snapshots_dir;
  base::PathService::Get(base::DIR_CACHE, &snapshots_dir);
  // Snapshots are located in a path with the bundle ID used twice.
  snapshots_dir = snapshots_dir.Append("Snapshots")
                      .Append(base::apple::BaseBundleID())
                      .Append(base::apple::BaseBundleID());
  const char* retina_suffix = "";
  CGFloat scale = [UIScreen mainScreen].scale;
  if (scale == 2) {
    retina_suffix = "@2x";
  } else if (scale == 3) {
    retina_suffix = "@3x";
  }
  for (unsigned int i = 0; i < std::size(kOrientationDescriptions); i++) {
    std::string snapshot_filename =
        base::StringPrintf("UIApplicationAutomaticSnapshotDefault-%s%s.png",
                           kOrientationDescriptions[i], retina_suffix);
    base::FilePath snapshot_path = snapshots_dir.Append(snapshot_filename);
    snapshots_paths->push_back(snapshot_path);
  }
}
