// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/recursive_operation_delegate.h"

#include <stddef.h>

#include "base/check_op.h"
#include "base/containers/queue.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"

namespace storage {

RecursiveOperationDelegate::RecursiveOperationDelegate(
    FileSystemContext* file_system_context)
    : file_system_context_(file_system_context) {}

RecursiveOperationDelegate::~RecursiveOperationDelegate() = default;

void RecursiveOperationDelegate::Cancel() {
  canceled_ = true;
  OnCancel();
}

void RecursiveOperationDelegate::StartRecursiveOperation(
    const FileSystemURL& root,
    ErrorBehavior error_behavior,
    StatusCallback callback) {
  DCHECK(pending_directory_stack_.empty());
  DCHECK(pending_files_.empty());

  error_behavior_ = error_behavior;
  callback_ = std::move(callback);

  TryProcessFile(root);
}

void RecursiveOperationDelegate::TryProcessFile(const FileSystemURL& root) {
  ProcessFile(root,
              base::BindOnce(&RecursiveOperationDelegate::DidTryProcessFile,
                             AsWeakPtr(), root));
}

FileSystemOperationRunner* RecursiveOperationDelegate::operation_runner() {
  return file_system_context_->operation_runner();
}

void RecursiveOperationDelegate::OnCancel() {}

void RecursiveOperationDelegate::DidTryProcessFile(const FileSystemURL& root,
                                                   base::File::Error error) {
  DCHECK(pending_directory_stack_.empty());
  DCHECK(pending_files_.empty());

  if (canceled_ || error != base::File::FILE_ERROR_NOT_A_FILE) {
    Done(error);
    return;
  }

  pending_directory_stack_.push(base::queue<FileSystemURL>());
  pending_directory_stack_.top().push(root);
  ProcessNextDirectory();
}

void RecursiveOperationDelegate::ProcessNextDirectory() {
  DCHECK(pending_files_.empty());
  DCHECK(!pending_directory_stack_.empty());
  DCHECK(!pending_directory_stack_.top().empty());

  const FileSystemURL& url = pending_directory_stack_.top().front();

  ProcessDirectory(
      url, base::BindOnce(&RecursiveOperationDelegate::DidProcessDirectory,
                          AsWeakPtr()));
}

void RecursiveOperationDelegate::DidProcessDirectory(base::File::Error error) {
  DCHECK(pending_files_.empty());
  DCHECK(!pending_directory_stack_.empty());
  DCHECK(!pending_directory_stack_.top().empty());

  if (canceled_ || error != base::File::FILE_OK) {
    if (canceled_ ||
        error_behavior_ == FileSystemOperation::ERROR_BEHAVIOR_ABORT) {
      Done(error);
      return;
    }
    SetPreviousError(error);
    // For ERROR_BEHAVIOR_SKIP, we skip processing the current directory and
    // proceed with the next.
    pending_directory_stack_.top().pop();
    ProcessSubDirectory();
    return;
  }

  const FileSystemURL& parent = pending_directory_stack_.top().front();
  pending_directory_stack_.push(base::queue<FileSystemURL>());
  operation_runner()->ReadDirectory(
      parent, base::BindRepeating(&RecursiveOperationDelegate::DidReadDirectory,
                                  AsWeakPtr(), parent));
}

void RecursiveOperationDelegate::DidReadDirectory(const FileSystemURL& parent,
                                                  base::File::Error error,
                                                  FileEntryList entries,
                                                  bool has_more) {
  DCHECK(!pending_directory_stack_.empty());

  if (canceled_ || error != base::File::FILE_OK) {
    Done(error);
    return;
  }

  for (size_t i = 0; i < entries.size(); i++) {
    FileSystemURL url = file_system_context_->CreateCrackedFileSystemURL(
        parent.storage_key(), parent.mount_type(),
        parent.virtual_path().Append(entries[i].name));
    if (parent.bucket().has_value())
      url.SetBucket(parent.bucket().value());
    if (entries[i].type == filesystem::mojom::FsFileType::DIRECTORY)
      pending_directory_stack_.top().push(url);
    else
      pending_files_.push(url);
  }

  // Wait for next entries.
  if (has_more)
    return;

  ProcessPendingFiles();
}

void RecursiveOperationDelegate::ProcessPendingFiles() {
  DCHECK(!pending_directory_stack_.empty());

  if (pending_files_.empty() || canceled_) {
    ProcessSubDirectory();
    return;
  }

  // Do not post any new tasks.
  if (canceled_)
    return;

  // Run ProcessFile.
  scoped_refptr<base::SingleThreadTaskRunner> current_task_runner =
      base::SingleThreadTaskRunner::GetCurrentDefault();
  if (!pending_files_.empty()) {
    current_task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            &RecursiveOperationDelegate::ProcessFile, AsWeakPtr(),
            pending_files_.front(),
            base::BindOnce(&RecursiveOperationDelegate::DidProcessFile,
                           AsWeakPtr(), pending_files_.front())));
    pending_files_.pop();
  }
}

void RecursiveOperationDelegate::DidProcessFile(const FileSystemURL& url,
                                                base::File::Error error) {
  if (error != base::File::FILE_OK) {
    if (error_behavior_ == FileSystemOperation::ERROR_BEHAVIOR_ABORT) {
      // If an error occurs, invoke Done immediately (even if there remain
      // running operations). It is because in the callback, this instance is
      // deleted.
      Done(error);
      return;
    }

    SetPreviousError(error);
  }

  ProcessPendingFiles();
}

void RecursiveOperationDelegate::ProcessSubDirectory() {
  DCHECK(pending_files_.empty());
  DCHECK(!pending_directory_stack_.empty());

  if (canceled_) {
    Done(base::File::FILE_ERROR_ABORT);
    return;
  }

  if (!pending_directory_stack_.top().empty()) {
    // There remain some sub directories. Process them first.
    ProcessNextDirectory();
    return;
  }

  // All subdirectories are processed.
  pending_directory_stack_.pop();
  if (pending_directory_stack_.empty()) {
    // All files/directories are processed.
    Done(base::File::FILE_OK);
    return;
  }

  DCHECK(!pending_directory_stack_.top().empty());
  PostProcessDirectory(
      pending_directory_stack_.top().front(),
      base::BindOnce(&RecursiveOperationDelegate::DidPostProcessDirectory,
                     AsWeakPtr()));
}

void RecursiveOperationDelegate::DidPostProcessDirectory(
    base::File::Error error) {
  DCHECK(pending_files_.empty());
  DCHECK(!pending_directory_stack_.empty());
  DCHECK(!pending_directory_stack_.top().empty());

  pending_directory_stack_.top().pop();
  if (canceled_ || error != base::File::FILE_OK) {
    if (canceled_ || error_behavior_ == ErrorBehavior::ERROR_BEHAVIOR_ABORT) {
      Done(error);
      return;
    }
    SetPreviousError(error);
  }

  ProcessSubDirectory();
}

void RecursiveOperationDelegate::SetPreviousError(base::File::Error error) {
  DCHECK_NE(error, base::File::FILE_OK);
  previous_error_ = error;
}

void RecursiveOperationDelegate::Done(base::File::Error error) {
  if (canceled_ && error == base::File::FILE_OK) {
    std::move(callback_).Run(base::File::FILE_ERROR_ABORT);
  } else {
    if (error != base::File::FILE_OK) {
      std::move(callback_).Run(error);
    } else {
      std::move(callback_).Run(previous_error_);
    }
  }
}

}  // namespace storage
