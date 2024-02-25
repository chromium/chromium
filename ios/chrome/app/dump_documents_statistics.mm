// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/dump_documents_statistics.h"

#import "base/apple/backup_util.h"
#import "base/apple/foundation_util.h"
#import "base/files/file.h"
#import "base/files/file_enumerator.h"
#import "base/files/file_path.h"
#import "base/files/file_util.h"
#import "base/i18n/time_formatting.h"
#import "base/json/json_writer.h"
#import "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
#import "base/time/time.h"

namespace documents_statistics {

// Converts time to a human readable string in the device's local time.
std::string TimeToLocalString(base::Time time) {
  return base::UnlocalizedTimeFormatWithPattern(time, "yyyy-MM-dd'T'HH:mm:ss");
}

// Gathers statistics for `root`, recusively if `root` is a directory.
base::Value::Dict CollectFileStatistics(base::FilePath root) {
  base::Value::Dict statistics;
  std::u16string name = root.BaseName().LossyDisplayName();
  statistics.Set("name", name);

  base::File file(root, base::File::FLAG_OPEN | base::File::FLAG_READ);
  base::File::Info info;
  if (!file.IsValid() || !file.GetInfo(&info)) {
    return statistics;
  }

  if (info.is_directory) {
    int64_t total_directory_size = 0;
    base::Value::List contents;

    base::FileEnumerator enumerator(
        root, /*recursive=*/false,
        base::FileEnumerator::DIRECTORIES | base::FileEnumerator::FILES);
    for (base::FilePath path = enumerator.Next(); !path.empty();
         path = enumerator.Next()) {
      base::Value::Dict dir_item_statistics =
          CollectFileStatistics(root.Append(path.BaseName()));
      auto size = dir_item_statistics.FindDouble("size");
      if (size) {
        total_directory_size += size.value();
      }

      contents.Append(std::move(dir_item_statistics));
    }
    statistics.Set("size", static_cast<double>(total_directory_size));
    statistics.Set("contents", std::move(contents));
  } else {
    statistics.Set("size", static_cast<double>(info.size));
  }
  statistics.Set("accessed", TimeToLocalString(info.last_accessed));
  statistics.Set("created", TimeToLocalString(info.creation_time));
  statistics.Set("modified", TimeToLocalString(info.last_modified));

  statistics.Set("excludedFromBackups", base::apple::GetBackupExclusion(root));

  return statistics;
}

// Gathers statistics for `root` and writes them to a new JSON file within
// `statistics_dir`.
void WriteSandboxStatisticsToFile(base::FilePath root,
                                  base::FilePath statistics_dir) {
  base::Value::Dict statistics = CollectFileStatistics(root);

  auto json = base::WriteJson(statistics);
  if (json) {
    if (!base::PathExists(statistics_dir)) {
      base::CreateDirectory(statistics_dir);
    }

    std::string file_name = TimeToLocalString(base::Time::Now()) + ".json";

    base::FilePath statistics_file_path = statistics_dir.Append(file_name);
    base::File statistics_file(
        statistics_file_path, base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    if (statistics_file.IsValid()) {
      std::string json_value = json.value();
      statistics_file.WriteAtCurrentPos(json_value.data(), json_value.size());
      statistics_file.Flush();
    } else {
      DLOG(ERROR) << "Statistics file path could not be opened.";
    }
  } else {
    DLOG(ERROR) << "Statistics could not be converted to JSON.";
  }
}

// Dumps statistics in JSON format for the user's entire Document directory.
void DumpSandboxFileStatistics() {
  base::FilePath documents_path = base::apple::GetUserDocumentPath();
  base::FilePath file_stats_directory =
      base::apple::GetUserDocumentPath().Append("sandboxFileStats");

  // Go up one directory from documents to include all surrounding directories.
  base::FilePath root = documents_path.DirName();

  base::ThreadPool::PostTask(FROM_HERE,
                             {base::MayBlock(), base::TaskPriority::BEST_EFFORT,
                              base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
                             base::BindOnce(&WriteSandboxStatisticsToFile, root,
                                            file_stats_directory));
}

}  // namespace documents_statistics
