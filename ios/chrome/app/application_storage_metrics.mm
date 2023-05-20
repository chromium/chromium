// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/application_storage_metrics.h"

#import <Foundation/Foundation.h>

#import "base/files/file_enumerator.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/mac/foundation_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/task/sequenced_task_runner.h"
#import "base/task/thread_pool.h"
#import "components/optimization_guide/core/optimization_guide_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// Key for last stored time that size metrics of the documents directory were
// logged.
NSString* const kLastApplicationStorageMetricsLogTime =
    @"LastApplicationStorageMetricsLogTime";

// Calculates and returns the total size used by `root`.
int64_t CalculateTotalSize(base::FilePath root) {
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

// Logs the "Documents" directory size. Accepts a task runner as a parameter in
// order to keep it in scope throughout the execution.
void LogDocumentsDirectorySize(scoped_refptr<base::SequencedTaskRunner>) {
  base::FilePath documents_path = base::mac::GetUserDocumentPath();
  int total_size_bytes = CalculateTotalSize(documents_path);
  UMA_HISTOGRAM_MEMORY_MEDIUM_MB("IOS.SandboxMetrics.DocumentsSize2",
                                 total_size_bytes / 1024 / 1024);
}

// Logs the "Library" directory size. Accepts a task runner as a parameter in
// order to keep it in scope throughout the execution.
void LogLibraryDirectorySize(scoped_refptr<base::SequencedTaskRunner>) {
  base::FilePath library_path = base::mac::GetUserLibraryPath();
  int total_size_bytes = CalculateTotalSize(library_path);
  UMA_HISTOGRAM_MEMORY_MEDIUM_MB("IOS.SandboxMetrics.LibrarySize",
                                 total_size_bytes / 1024 / 1024);
}

// Logs the optimization guide model downloads directory size. Accepts a task
// runner as a parameter in order to keep it in scope throughout the execution.
void LogOptimizationGuideModelDownloadsMetrics(
    base::FilePath profile_path,
    scoped_refptr<base::SequencedTaskRunner>) {
  int items = 0;

  base::FilePath models_dir = profile_path.Append(
      optimization_guide::kOptimizationGuidePredictionModelDownloads);
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

// Updates the last metric logged time. Accepts a task runner as a parameter in
// order to keep it in scope throughout the execution.
void UpdateLastLoggedTime(scoped_refptr<base::SequencedTaskRunner>) {
  [[NSUserDefaults standardUserDefaults]
      setObject:[NSDate date]
         forKey:kLastApplicationStorageMetricsLogTime];
}

void LogApplicationStorageMetrics(base::FilePath profile_path) {
  scoped_refptr<base::SequencedTaskRunner> task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&LogDocumentsDirectorySize, task_runner));
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&LogLibraryDirectorySize, task_runner));
  task_runner->PostTask(
      FROM_HERE, base::BindOnce(&LogOptimizationGuideModelDownloadsMetrics,
                                profile_path, task_runner));
  task_runner->PostTask(FROM_HERE,
                        base::BindOnce(&UpdateLastLoggedTime, task_runner));
}
