// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "storage/browser/file_system/remove_operation_delegate.h"

#include "base/functional/bind.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation_runner.h"

namespace storage {

RemoveOperationDelegate::RemoveOperationDelegate(
    FileSystemContext* file_system_context,
    const FileSystemURL& url,
    StatusCallback callback)
    : RecursiveOperationDelegate(file_system_context),
      url_(url),
      callback_(std::move(callback)) {}

RemoveOperationDelegate::~RemoveOperationDelegate() = default;

void RemoveOperationDelegate::Run() {
#if DCHECK_IS_ON()
  DCHECK(!did_run_);
  did_run_ = true;
#endif
  operation_runner()->RemoveFile(
      url_, base::BindOnce(&RemoveOperationDelegate::DidTryRemoveFile,
                           weak_factory_.GetWeakPtr()));
}

void RemoveOperationDelegate::RunRecursively() {
#if DCHECK_IS_ON()
  DCHECK(!did_run_);
  did_run_ = true;
#endif
  StartRecursiveOperation(url_, FileSystemOperation::ERROR_BEHAVIOR_ABORT,
                          std::move(callback_));
}

void RemoveOperationDelegate::ProcessFile(const FileSystemURL& url,
                                          StatusCallback callback) {
  operation_runner()->RemoveFile(
      url, base::BindOnce(&RemoveOperationDelegate::DidRemoveFile,
                          weak_factory_.GetWeakPtr(), std::move(callback)));
}

void RemoveOperationDelegate::ProcessDirectory(const FileSystemURL& url,
                                               StatusCallback callback) {
  std::move(callback).Run(base::File::FILE_OK);
}

void RemoveOperationDelegate::PostProcessDirectory(const FileSystemURL& url,
                                                   StatusCallback callback) {
  operation_runner()->RemoveDirectory(url, std::move(callback));
}

base::WeakPtr<RecursiveOperationDelegate> RemoveOperationDelegate::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void RemoveOperationDelegate::DidTryRemoveFile(base::File::Error error) {
  if (error != base::File::FILE_ERROR_NOT_A_FILE &&
      error != base::File::FILE_ERROR_SECURITY) {
    std::move(callback_).Run(error);
    return;
  }
  operation_runner()->RemoveDirectory(
      url_, base::BindOnce(&RemoveOperationDelegate::DidTryRemoveDirectory,
                           weak_factory_.GetWeakPtr(), error));
}

void RemoveOperationDelegate::DidTryRemoveDirectory(
    base::File::Error remove_file_error,
    base::File::Error remove_directory_error) {
  std::move(callback_).Run(remove_directory_error ==
                                   base::File::FILE_ERROR_NOT_A_DIRECTORY
                               ? remove_file_error
                               : remove_directory_error);
}

void RemoveOperationDelegate::DidRemoveFile(StatusCallback callback,
                                            base::File::Error error) {
  if (error == base::File::FILE_ERROR_NOT_FOUND) {
    std::move(callback).Run(base::File::FILE_OK);
    return;
  }
  std::move(callback).Run(error);
}

}  // namespace storage
