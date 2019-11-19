// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/snapshots/snapshots_util.h"

#import <UIKit/UIKit.h>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/mac/foundation_util.h"
#include "base/path_service.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/post_task.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
const char* kOrientationDescriptions[] = {
    "LandscapeLeft",
    "LandscapeRight",
    "Portrait",
    "PortraitUpsideDown",
};

// Delete all files in |paths|.
void DeleteAllFiles(std::vector<base::FilePath> paths) {
  for (const auto& path : paths) {
    ignore_result(base::DeleteFile(path, false));
  }
}
}  // namespace

void ClearIOSSnapshots(base::OnceClosure callback) {
  // Generates a list containing all the possible snapshot paths because the
  // list of snapshots stored on the device can't be obtained programmatically.
  std::vector<base::FilePath> snapshots_paths;
  GetSnapshotsPaths(&snapshots_paths);
  base::PostTaskAndReply(
      FROM_HERE,
      {base::ThreadPool(), base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&DeleteAllFiles, std::move(snapshots_paths)),
      std::move(callback));
}

void GetSnapshotsPaths(std::vector<base::FilePath>* snapshots_paths) {
  DCHECK(snapshots_paths);
  base::FilePath snapshots_dir;
  base::PathService::Get(base::DIR_CACHE, &snapshots_dir);
  // Snapshots are located in a path with the bundle ID used twice.
  snapshots_dir = snapshots_dir.Append("Snapshots")
                      .Append(base::mac::BaseBundleID())
                      .Append(base::mac::BaseBundleID());
  const char* retina_suffix = "";
  CGFloat scale = [UIScreen mainScreen].scale;
  if (scale == 2) {
    retina_suffix = "@2x";
  } else if (scale == 3) {
    retina_suffix = "@3x";
  }
  for (unsigned int i = 0; i < base::size(kOrientationDescriptions); i++) {
    std::string snapshot_filename =
        base::StringPrintf("UIApplicationAutomaticSnapshotDefault-%s%s.png",
                           kOrientationDescriptions[i], retina_suffix);
    base::FilePath snapshot_path = snapshots_dir.Append(snapshot_filename);
    snapshots_paths->push_back(snapshot_path);
  }
}
