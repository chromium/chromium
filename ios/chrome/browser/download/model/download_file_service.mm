// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/download/model/download_file_service.h"

#import <Foundation/Foundation.h>

#import "base/files/file_util.h"
#import "base/functional/bind.h"
#import "base/strings/string_number_conversions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
#import "base/uuid.h"
#import "ios/chrome/browser/download/model/download_directory_util.h"
#import "ios/chrome/browser/download/model/download_record_service.h"

DownloadFileService::DownloadFileService(
    DownloadRecordService* download_record_service)
    : file_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      download_record_service_(download_record_service) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
}

DownloadFileService::~DownloadFileService() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);
}

void DownloadFileService::MoveDownloadFile(
    const std::string& download_id,
    const base::FilePath& source_path,
    const base::FilePath& destination_path,
    MoveCompleteCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  if (download_id.empty() || source_path.empty() || destination_path.empty()) {
    if (callback) {
      std::move(callback).Run(false, download_id, source_path,
                              destination_path);
    }
    return;
  }

  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DownloadFileService::DoMoveFileOnBackgroundThread,
                     file_task_runner_, source_path, destination_path),
      base::BindOnce(&DownloadFileService::OnFileMoveComplete,
                     weak_ptr_factory_.GetWeakPtr(), download_id, source_path,
                     destination_path, std::move(callback)));
}

void DownloadFileService::ResolveAvailableFilePath(
    const base::FilePath& target_directory,
    const base::FilePath& suggested_filename,
    base::OnceCallback<void(base::FilePath)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&DownloadFileService::FindAvailableDownloadFilePath,
                     file_task_runner_, target_directory, suggested_filename),
      std::move(callback));
}

void DownloadFileService::CheckFileExists(
    const base::FilePath& file_path,
    base::OnceCallback<void(bool)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  file_task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce(base::PathExists, file_path),
      std::move(callback));
}

void DownloadFileService::OnFileMoveComplete(
    const std::string& download_id,
    const base::FilePath& source_path,
    const base::FilePath& destination_path,
    MoveCompleteCallback callback,
    bool move_success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(main_sequence_checker_);

  if (move_success && download_record_service_) {
    // Convert absolute path to relative path for storage.
    base::FilePath relative_path =
        ConvertToRelativeDownloadPath(destination_path);
    download_record_service_->UpdateDownloadFilePathAsync(download_id,
                                                          relative_path);
  }

  if (callback) {
    std::move(callback).Run(move_success, download_id, source_path,
                            destination_path);
  }
}

bool DownloadFileService::DoMoveFileOnBackgroundThread(
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    const base::FilePath& source_path,
    const base::FilePath& destination_path) {
  DCHECK(file_task_runner->RunsTasksInCurrentSequence());

  // Check if source file exists.
  if (!base::PathExists(source_path)) {
    return false;
  }

  // Create destination directory if it doesn't exist.
  base::FilePath destination_dir = destination_path.DirName();
  if (!base::CreateDirectory(destination_dir)) {
    return false;
  }

  // Move the file.
  if (!base::Move(source_path, destination_path)) {
    return false;
  }

  return true;
}

base::FilePath DownloadFileService::FindAvailableDownloadFilePath(
    scoped_refptr<base::SequencedTaskRunner> file_task_runner,
    const base::FilePath& download_dir,
    const base::FilePath& file_name) {
  DCHECK(file_task_runner->RunsTasksInCurrentSequence());

  base::FilePath actual_file_name = file_name;
  // If the suggested `file_name` is empty or '.' or '..' then it is replaced
  // with a randomly generated UUID.
  if (actual_file_name.empty() ||
      actual_file_name.value() == base::FilePath::kCurrentDirectory ||
      actual_file_name.value() == base::FilePath::kParentDirectory) {
    actual_file_name =
        base::FilePath(base::Uuid::GenerateRandomV4().AsLowercaseString());
  }

  base::FilePath candidate_file_name = actual_file_name;
  int number_of_attempts = 0;
  while (base::PathExists(download_dir.Append(candidate_file_name))) {
    number_of_attempts++;
    candidate_file_name = actual_file_name.InsertBeforeExtension(
        " (" + base::NumberToString(number_of_attempts) + ")");
  }
  return download_dir.Append(candidate_file_name);
}
