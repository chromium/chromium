// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_storage_metrics.h"

#import <Foundation/Foundation.h>

#import "base/apple/foundation_util.h"
#import "base/base_paths.h"
#import "base/files/file_enumerator.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/path_service.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "components/history/core/browser/history_constants.h"
#import "components/optimization_guide/core/optimization_guide_constants.h"
#import "ios/chrome/browser/shared/model/paths/paths.h"

// The etension used for all snapshot images.
constexpr std::string_view kSnapshotImageExtension = ".jpg";
// The label appended to the snapshot filename for grey snapshot images.
constexpr std::string_view kGreySnapshotImageIdentifier = "Grey";

// The path, relative to the profile directory, where tab state is stored.
const base::FilePath::CharType kSessionsPath[] = FILE_PATH_LITERAL("Sessions");

// The path, relative to the profile directory, where snapshots are stored.
const base::FilePath::CharType kSnapshotsPath[] =
    FILE_PATH_LITERAL("Snapshots");

// The path, relative to the application's Library directory, to WebKit's
// storage location for website local data .
const base::FilePath::CharType kWebsiteLocalDataPath[] =
    FILE_PATH_LITERAL("WebKit/WebsiteData/Default");

// The path, relative to the tmp directory, used for WebKit tmp data.
const base::FilePath::CharType kWebKitTmpPath[] = FILE_PATH_LITERAL("WebKit");

// The path, relative to the Caches directory, used for WebKit cache.
const base::FilePath::CharType kWebKitCachePath[] = FILE_PATH_LITERAL("WebKit");

struct DirectorySnapshotDetails {
  DirectorySnapshotDetails() : snapshot_count(0), total_size_bytes(0) {}

  int snapshot_count;
  int64_t total_size_bytes;
};

DirectorySnapshotDetails CalculateImageMetricsInRoot(bool grey_only,
                                                     base::FilePath root) {
  DirectorySnapshotDetails details;

  if (!base::PathExists(root)) {
    return details;
  }

  base::File file(root, base::File::FLAG_OPEN | base::File::FLAG_READ);
  base::File::Info info;
  if (!file.IsValid() || !file.GetInfo(&info)) {
    return details;
  }

  if (!info.is_directory) {
    if (root.MatchesExtension(kSnapshotImageExtension)) {
      // Add this snapshot to the total if search for all snapshots or searching
      // for grey snapshots only and the filename contains
      // `kGreySnapshotImageIdentifier`.
      if (!grey_only ||
          root.BaseName().MaybeAsASCII().find(kGreySnapshotImageIdentifier) !=
              std::string::npos) {
        details.snapshot_count += 1;
        details.total_size_bytes += info.size;
      }
    }
    return details;
  }

  base::FileEnumerator enumerator(
      root, /*recursive=*/false,
      base::FileEnumerator::DIRECTORIES | base::FileEnumerator::FILES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    DirectorySnapshotDetails child_details =
        CalculateImageMetricsInRoot(grey_only, root.Append(path.BaseName()));
    details.snapshot_count += child_details.snapshot_count;
    details.total_size_bytes += child_details.total_size_bytes;
  }

  return details;
}
// Calculates and returns the total size used by `root`.
int64_t CalculateTotalSize(base::FilePath root) {
  if (!base::PathExists(root)) {
    return 0;
  }

  base::File file(root, base::File::FLAG_OPEN | base::File::FLAG_READ);
  base::File::Info info;
  if (!file.IsValid() || !file.GetInfo(&info)) {
    return 0;
  }

  if (!info.is_directory) {
    return info.size;
  }

  int64_t total_directory_size = 0;

  base::FileEnumerator enumerator(
      root, /*recursive=*/false,
      base::FileEnumerator::DIRECTORIES | base::FileEnumerator::FILES);
  for (base::FilePath path = enumerator.Next(); !path.empty();
       path = enumerator.Next()) {
    int64_t dir_item_size = CalculateTotalSize(root.Append(path.BaseName()));
    total_directory_size += dir_item_size;
  }
  return total_directory_size;
}

// Returns the path to the sandbox "Application Support" directory.
base::FilePath GetApplicationSupportDirectory() {
  base::FilePath application_support_path;
  base::PathService::Get(base::DIR_APP_DATA, &application_support_path);
  return application_support_path;
}

// Returns the path to the sandbox "Caches" directory.
base::FilePath GetCachesDirectory() {
  NSArray* cachesDirectories = NSSearchPathForDirectoriesInDomains(
      NSCachesDirectory, NSUserDomainMask, YES);
  NSString* cachePath = [cachesDirectories objectAtIndex:0];
  return base::apple::NSStringToFilePath(cachePath);
}

// Logs the WebKit and Chrome Cache directory sizes. Accepts a task runner as a
// parameter in order to keep it in scope throughout the execution.
void LogCacheDirectorySizes(scoped_refptr<base::SequencedTaskRunner>) {
  base::FilePath caches_path = GetCachesDirectory();

  base::FilePath webkit_caches_path = caches_path.Append(kWebKitCachePath);
  int64_t webkit_cache_size_bytes = CalculateTotalSize(webkit_caches_path);
  UMA_HISTOGRAM_MEMORY_MEDIUM_MB("IOS.SandboxMetrics.WebKitCacheSize",
                                 webkit_cache_size_bytes / 1024 / 1024);

  int64_t total_cache_size_bytes = CalculateTotalSize(caches_path);
  int64_t chrome_cache_size_bytes =
      total_cache_size_bytes - webkit_cache_size_bytes;
  UMA_HISTOGRAM_MEMORY_MEDIUM_MB("IOS.SandboxMetrics.ChromeCacheSize",
                                 chrome_cache_size_bytes / 1024 / 1024);
}

// Logs the "Documents" directory size. Accepts a task runner as a parameter in
// order to keep it in scope throughout the execution.
void LogDocumentsDirectorySize(scoped_refptr<base::SequencedTaskRunner>) {
  base::FilePath documents_path = base::apple::GetUserDocumentPath();
  int total_size_bytes = CalculateTotalSize(documents_path);
  UMA_HISTOGRAM_MEMORY_MEDIUM_MB("IOS.SandboxMetrics.DocumentsSize2",
                                 total_size_bytes / 1024 / 1024);
}

// Logs the Favicons storage size. Accepts a task runner as a parameter in
// order to keep it in scope throughout the execution.
void LogFaviconsStorageSize(base::FilePath profile_path,
                            scoped_refptr<base::SequencedTaskRunner>) {
  base::FilePath favicons_path =
      profile_path.Append(history::kFaviconsFilename);
  int64_t total_size_bytes = CalculateTotalSize(favicons_path);
  UMA_HISTOGRAM_MEMORY_KB("IOS.SandboxMetrics.FaviconsSize",
                          total_size_bytes / 1024);
}

// Logs the "Library" directory size. Accepts a task runner as a parameter in
// order to keep it in scope throughout the execution.
void LogLibraryDirectorySize(scoped_refptr<base::SequencedTaskRunner>) {
  base::FilePath library_path = base::apple::GetUserLibraryPath();
  int total_size_bytes = CalculateTotalSize(library_path);
  UMA_HISTOGRAM_MEMORY_MEDIUM_MB("IOS.SandboxMetrics.LibrarySize",
                                 total_size_bytes / 1024 / 1024);
}

// Logs the optimization guide model downloads directory size. Accepts a task
// runner as a parameter in order to keep it in scope throughout the execution.
void LogOptimizationGuideModelDownloadsMetrics(
    scoped_refptr<base::SequencedTaskRunner>) {
  int items = 0;

  base::FilePath models_dir =
      base::PathService::CheckedGet(ios::DIR_USER_DATA)
          .Append(optimization_guide::kOptimizationGuideModelStoreDirPrefix);
  if (base::PathExists(models_dir)) {
    int total_size_bytes = CalculateTotalSize(models_dir);
    UMA_HISTOGRAM_MEMORY_MEDIUM_MB(
        "IOS.SandboxMetrics.OptimizationGuideModelDownloadsSize",
        total_size_bytes / 1024 / 1024);

    base::FileEnumerator enumerator(models_dir, /*recursive=*/false,
                                    base::FileEnumerator::DIRECTORIES);
    for (base::FilePath path = enumerator.Next(); !path.empty();
         path = enumerator.Next()) {
      items++;
    }
  }

  UMA_HISTOGRAM_COUNTS_1000(
      "IOS.SandboxMetrics.OptimizationGuideModelDownloadedItems", items);
}

// Logs the total amount of storage used by the regular and OTR tabs for both
// tab state and snapshots and the total size of the Application Support
// directory excluding the storage used by tabs.
void LogApplicationSupportDirectorySize(
    base::FilePath profile_path,
    base::FilePath otr_profile_path,
    scoped_refptr<base::SequencedTaskRunner>) {
  base::FilePath session_storage_dir = profile_path.Append(kSessionsPath);
  int64_t regular_tabs_size_bytes = CalculateTotalSize(session_storage_dir);
  UMA_HISTOGRAM_MEMORY_MEDIUM_MB("IOS.SandboxMetrics.TotalRegularSessionSize",
                                 regular_tabs_size_bytes / 1024 / 1024);

  base::FilePath otr_storage_dir = otr_profile_path.Append(kSessionsPath);
  int64_t otr_tabs_size_bytes = CalculateTotalSize(otr_storage_dir);
  UMA_HISTOGRAM_MEMORY_MEDIUM_MB("IOS.SandboxMetrics.TotalOTRSessionSize",
                                 otr_tabs_size_bytes / 1024 / 1024);

  int64_t tab_storage_size = regular_tabs_size_bytes + otr_tabs_size_bytes;
  int64_t application_support_size =
      CalculateTotalSize(GetApplicationSupportDirectory());
  int64_t size_without_tabs_data = application_support_size - tab_storage_size;
  UMA_HISTOGRAM_MEMORY_MEDIUM_MB("IOS.SandboxMetrics.ApplicationSupportSize",
                                 size_without_tabs_data / 1024 / 1024);
}

void LogAverageSnapshotSizes(base::FilePath profile_path,
                             scoped_refptr<base::SequencedTaskRunner>) {
  base::FilePath snapshots_storage_dir = profile_path.Append(kSnapshotsPath);

  DirectorySnapshotDetails grey_snapshot_details =
      CalculateImageMetricsInRoot(/*grey_only=*/true, snapshots_storage_dir);
  int64_t grey_average_bytes_size = 0;
  if (grey_snapshot_details.snapshot_count > 0) {
    grey_average_bytes_size = grey_snapshot_details.total_size_bytes /
                              grey_snapshot_details.snapshot_count;
  }
  UMA_HISTOGRAM_MEMORY_KB("IOS.SandboxMetrics.AverageGreySnapshotSize",
                          grey_average_bytes_size / 1024);

  DirectorySnapshotDetails all_snapshot_details =
      CalculateImageMetricsInRoot(/*grey_only=*/false, snapshots_storage_dir);
  int64_t color_total_size_bytes = all_snapshot_details.total_size_bytes -
                                   grey_snapshot_details.total_size_bytes;
  int color_snapshot_count = all_snapshot_details.snapshot_count -
                             grey_snapshot_details.snapshot_count;
  int64_t color_average_bytes_size = 0;
  if (color_snapshot_count > 0) {
    color_average_bytes_size = color_total_size_bytes / color_snapshot_count;
  }
  UMA_HISTOGRAM_MEMORY_KB("IOS.SandboxMetrics.AverageColorSnapshotSize",
                          color_average_bytes_size / 1024);
}

// Logs the WebKit tmp directory size and the size of the tmp directory
// excluding the WebKit directory. Accepts a task runner as a parameter in order
// to keep it in scope throughout the execution.
void LogTmpDirectorySizes(base::FilePath profile_path,
                          scoped_refptr<base::SequencedTaskRunner>) {
  base::FilePath tmp_dir;
  if (GetTempDir(&tmp_dir)) {
    base::FilePath tmp_webkit_path = profile_path.Append(kWebKitTmpPath);
    int webkit_tmp_size_bytes = CalculateTotalSize(tmp_webkit_path);
    UMA_HISTOGRAM_MEMORY_MEDIUM_MB("IOS.SandboxMetrics.WebKitTempSize",
                                   webkit_tmp_size_bytes / 1024 / 1024);

    int total_tmp_size_bytes = CalculateTotalSize(tmp_dir);
    int tmp_without_webkit_data = total_tmp_size_bytes - webkit_tmp_size_bytes;
    UMA_HISTOGRAM_MEMORY_MEDIUM_MB("IOS.SandboxMetrics.ChromeTempSize",
                                   tmp_without_webkit_data / 1024 / 1024);
  }
}

// Logs the `kWebsiteLocalDataPath` directory size. Accepts a task runner as a
// parameter in order to keep it in scope throughout the execution.
void LogWebsiteLocalDataSize(base::FilePath profile_path,
                             scoped_refptr<base::SequencedTaskRunner>) {
  base::FilePath library_path = base::apple::GetUserLibraryPath();
  base::FilePath website_data_dir = profile_path.Append(kWebsiteLocalDataPath);
  int total_size_bytes = CalculateTotalSize(website_data_dir);
  UMA_HISTOGRAM_MEMORY_MEDIUM_MB("IOS.SandboxMetrics.WebsiteLocalData",
                                 total_size_bytes / 1024 / 1024);
}

void LogApplicationStorageMetrics(base::FilePath profile_path,
                                  base::FilePath off_the_record_state_path) {
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&LogApplicationSupportDirectorySize, profile_path,
                     off_the_record_state_path, task_runner));
  task_runner->PostTask(FROM_HERE, base::BindOnce(&LogAverageSnapshotSizes,
                                                  profile_path, task_runner));
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&LogCacheDirectorySizes, task_runner));
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&LogDocumentsDirectorySize, task_runner));
  task_runner->PostTask(FROM_HERE, base::BindOnce(&LogFaviconsStorageSize,
                                                  profile_path, task_runner));
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&LogLibraryDirectorySize, task_runner));
  task_runner->PostTask(
      FROM_HERE,
      base::BindOnce(&LogOptimizationGuideModelDownloadsMetrics, task_runner));
  task_runner->PostTask(FROM_HERE, base::BindOnce(&LogTmpDirectorySizes,
                                                  profile_path, task_runner));
  task_runner->PostTask(FROM_HERE, base::BindOnce(&LogWebsiteLocalDataSize,
                                                  profile_path, task_runner));
}
