// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/crash/crash_directory_watcher.h"

#include <string>

#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"

namespace remoting {

namespace {

const base::FilePath::CharType kDumpExtension[] = FILE_PATH_LITERAL("dmp");
const base::FilePath::CharType kJsonExtension[] = FILE_PATH_LITERAL("json");

// Dmp files are of the form: crash_directory/<crash_guid>.dmp
// Metadata files are of the form: crash_directory/<crash_guid>.json
// Upload directory is of the form: crash_directory/<crash_guid>/
bool PrepareFilesForUpload(const base::FilePath& crash_directory,
                           const base::FilePath& crash_guid) {
  base::FilePath minidump_file =
      crash_directory.Append(crash_guid).AddExtension(kDumpExtension);
  if (!base::PathExists(minidump_file)) {
    LOG(WARNING) << "Minidump not found for crash: " << crash_guid;
    return false;
  }

  base::FilePath upload_directory = crash_directory.Append(crash_guid);
  if (base::PathExists(upload_directory)) {
    LOG(ERROR) << "Upload directory already exists for report: " << crash_guid;
    return false;
  }

  // We have a minidump and metadata file for this crash report so move them
  // into a sub-directory so they can be uploaded.
  base::File::Error error;
  if (!base::CreateDirectoryAndGetError(upload_directory, &error)) {
    LOG(ERROR) << "Failed to create directory for crash report: " << crash_guid
               << " due to error: " << error;
    return false;
  }

  base::FilePath metadata_file =
      crash_directory.Append(crash_guid).AddExtension(kJsonExtension);
  if (!base::Move(metadata_file,
                  upload_directory.Append(metadata_file.BaseName()))) {
    LOG(ERROR) << "Failed to move crash json file into upload directory for "
               << "crash: " << crash_guid;
    return false;
  }

  if (!base::Move(minidump_file,
                  upload_directory.Append(minidump_file.BaseName()))) {
    LOG(ERROR) << "Failed to move minidump file into upload directory for "
               << "crash: " << crash_guid;
    return false;
  }

  return true;
}

void DeleteCrashFiles(const base::FilePath& crash_directory,
                      const base::FilePath& crash_guid) {
  base::FilePath minidump_file =
      crash_directory.Append(crash_guid).AddExtension(kDumpExtension);
  if (!base::DeleteFile(minidump_file)) {
    PLOG(ERROR) << "Failed to delete " << minidump_file;
  }
  base::FilePath metadata_file =
      crash_directory.Append(crash_guid).AddExtension(kJsonExtension);
  if (!base::DeleteFile(metadata_file)) {
    PLOG(ERROR) << "Failed to delete " << metadata_file;
  }
}

}  // namespace

CrashDirectoryWatcher::CrashDirectoryWatcher() = default;
CrashDirectoryWatcher::~CrashDirectoryWatcher() = default;

void CrashDirectoryWatcher::Watch(base::FilePath crash_directory,
                                  UploadCallback callback) {
  LOG(INFO) << "Watching for crash minidumps in: " << crash_directory;
  DCHECK(!upload_callback_);
  upload_callback_ = std::move(callback);

  // TODO(joedow): Check |directory_to_watch| to see if there are any pending
  // uploads (i.e. crash_guid directories with a dump file) and run the callback
  // for their IDs.

  // Run a check when Watch() is first called in case there are DMP files left
  // over from a previous run.
  OnFileChangeDetected(crash_directory, false);

  // Unretained is sound as |file_path_watcher_| is owned by this instance and
  // the callback is run synchronously.
  // FilePathWatcher is an old class and isn't well documented (i.e. some of the
  // comments are contradictory and don't match reality). In our case, we are
  // using kNonRecursive which will trigger the callback for any changes in the
  // watched path on Linux and Windows. If this class is reused on other
  // platforms, we will need to confirm what the behavior is on the new
  // platform(s).
  file_path_watcher_.Watch(
      std::move(crash_directory), base::FilePathWatcher::Type::kNonRecursive,
      base::BindRepeating(&CrashDirectoryWatcher::OnFileChangeDetected,
                          base::Unretained(this)));
}

void CrashDirectoryWatcher::OnFileChangeDetected(const base::FilePath& path,
                                                 bool error) {
  if (error) {
    LOG(ERROR) << "Error watching crash directory: " << path;
    // TODO(joedow): Consider terminating the process so the folder to watch can
    // be recreated with the appropriate permissions.
    return;
  }

  // When the process crashes, a dmp file will be written followed by a metadata
  // json file. Thus if the metadata file exists we can assume the dmp is also
  // present and ready for upload.
  // We need to watch for new metadata files in the crash directory but not in
  // any descendants (sub-directories) as this instance will move the dmp and
  // json file to a new directory when they are ready to be uploaded and we
  // don't want to enumerate any files which have already been moved.
  base::FileEnumerator metadata_file_enumerator(
      path, /*recursive=*/false, base::FileEnumerator::FileType::FILES,
      FILE_PATH_LITERAL("*.json"));

  std::vector<base::FilePath> crash_guids;
  base::FilePath metadata_file = metadata_file_enumerator.Next();
  while (!metadata_file.empty()) {
    base::FilePath crash_guid = metadata_file.BaseName().RemoveExtension();
    LOG(INFO) << "Found new crash report: " << crash_guid;
    crash_guids.push_back(std::move(crash_guid));
    metadata_file = metadata_file_enumerator.Next();
  }

  for (const auto& crash_guid : crash_guids) {
    if (PrepareFilesForUpload(path, crash_guid)) {
      LOG(INFO) << "Crash report ready for upload: " << crash_guid;
      upload_callback_.Run(crash_guid);
    } else {
      LOG(ERROR) << "Deleting invalid crash report files for " << crash_guid;
      DeleteCrashFiles(path, crash_guid);
    }
  }

  auto last_error = metadata_file_enumerator.GetError();
  LOG_IF(WARNING, last_error != base::File::FILE_OK)
      << "File enumeration failed: " << last_error;
}

}  // namespace remoting
