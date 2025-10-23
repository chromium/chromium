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

constexpr std::array<std::string_view, 4> kOrientationDescriptions = {
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

// Returns the retina suffix.
std::string_view GetRetinaSuffix() {
  CGFloat scale = [UIScreen mainScreen].scale;
  if (scale == 2) {
    return "@2x";
  }

  if (scale == 3) {
    return "@3x";
  }

  return {};
}

}  // namespace

void ClearIOSSnapshots(base::OnceClosure callback) {
  // Generates a list containing all the possible snapshot paths because the
  // list of snapshots stored on the device can't be obtained programmatically.
  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&DeleteAllFiles, GetSnapshotsPaths()),
      std::move(callback));
}

std::vector<base::FilePath> GetSnapshotsPaths() {
  base::FilePath snapshots_dir;
  base::PathService::Get(base::DIR_CACHE, &snapshots_dir);
  const std::string_view retina_suffix = GetRetinaSuffix();

  // Snapshots are located in a path with the bundle ID used twice.
  snapshots_dir = snapshots_dir.Append("Snapshots")
                      .Append(base::apple::BaseBundleID())
                      .Append(base::apple::BaseBundleID());

  std::vector<base::FilePath> snapshots_paths;
  snapshots_paths.reserve(kOrientationDescriptions.size());

  for (std::string_view orientation_description : kOrientationDescriptions) {
    std::string snapshot_filename =
        base::StringPrintf("UIApplicationAutomaticSnapshotDefault-%s%s.png",
                           orientation_description, retina_suffix);
    base::FilePath snapshot_path = snapshots_dir.Append(snapshot_filename);
    snapshots_paths.push_back(std::move(snapshot_path));
  }

  return snapshots_paths;
}
