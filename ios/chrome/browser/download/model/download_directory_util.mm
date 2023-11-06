// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_directory_util.h"

#import "base/apple/foundation_util.h"
#import "base/feature_list.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "ios/web/common/features.h"

namespace {

// Synchronously deletes downloads directory.
void DeleteTempDownloadsDirectorySync() {
  base::FilePath downloads_directory;
  if (GetTempDownloadsDirectory(&downloads_directory)) {
    DeletePathRecursively(downloads_directory);
  }
}
}  // namespace

bool GetTempDownloadsDirectory(base::FilePath* directory_path) {
  if (!GetTempDir(directory_path)) {
    return false;
  }
  *directory_path = directory_path->Append("downloads");
  return true;
}

void GetDownloadsDirectory(base::FilePath* directory_path) {
  *directory_path =
      base::apple::NSStringToFilePath([NSSearchPathForDirectoriesInDomains(
          NSDocumentDirectory, NSAllDomainsMask, YES) firstObject]);
}

void DeleteTempDownloadsDirectory() {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&DeleteTempDownloadsDirectorySync));
}
