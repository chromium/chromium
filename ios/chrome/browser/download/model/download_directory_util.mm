// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_directory_util.h"

#import "base/apple/foundation_util.h"
#import "base/check.h"
#import "base/feature_list.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/task/task_traits.h"
#import "base/task/thread_pool.h"
#import "ios/web/common/features.h"

namespace {

// Override for testing purposes
base::FilePath* g_downloads_directory_for_testing = nullptr;

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
  // Use override for testing if set.
  if (g_downloads_directory_for_testing) {
    *directory_path = *g_downloads_directory_for_testing;
    return;
  }

  *directory_path =
      base::apple::NSStringToFilePath([NSSearchPathForDirectoriesInDomains(
          NSDocumentDirectory, NSAllDomainsMask, YES) firstObject]);
}

base::FilePath ConvertToRelativeDownloadPath(
    const base::FilePath& absolute_path) {
  DCHECK(absolute_path.IsAbsolute());

  base::FilePath downloads_directory;
  GetDownloadsDirectory(&downloads_directory);

  base::FilePath relative_path;
  bool success =
      downloads_directory.AppendRelativePath(absolute_path, &relative_path);
  DCHECK(success);

  return relative_path;
}

base::FilePath ConvertToAbsoluteDownloadPath(
    const base::FilePath& relative_path) {
  DCHECK(!relative_path.IsAbsolute());

  base::FilePath downloads_directory;
  GetDownloadsDirectory(&downloads_directory);

  return downloads_directory.Append(relative_path);
}

void DeleteTempDownloadsDirectory() {
  base::ThreadPool::PostTask(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce(&DeleteTempDownloadsDirectorySync));
}

namespace test {
void SetDownloadsDirectoryForTesting(const base::FilePath* directory) {
  g_downloads_directory_for_testing = const_cast<base::FilePath*>(directory);
}
}  // namespace test
