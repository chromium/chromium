// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_REMOVE_OPERATION_DELEGATE_H_
#define STORAGE_BROWSER_FILE_SYSTEM_REMOVE_OPERATION_DELEGATE_H_

#include "storage/browser/file_system/recursive_operation_delegate.h"

namespace storage {

class RemoveOperationDelegate final : public RecursiveOperationDelegate {
 public:
  RemoveOperationDelegate(FileSystemContext* file_system_context,
                          const FileSystemURL& url,
                          StatusCallback callback);

  RemoveOperationDelegate(const RemoveOperationDelegate&) = delete;
  RemoveOperationDelegate& operator=(const RemoveOperationDelegate&) = delete;

  ~RemoveOperationDelegate() override;

  // RecursiveOperationDelegate overrides:
  void Run() override;
  void RunRecursively() override;
  void ProcessFile(const FileSystemURL& url, StatusCallback callback) override;
  void ProcessDirectory(const FileSystemURL& url,
                        StatusCallback callback) override;
  void PostProcessDirectory(const FileSystemURL& url,
                            StatusCallback callback) override;
  base::WeakPtr<RecursiveOperationDelegate> AsWeakPtr() override;

 private:
  void DidTryRemoveFile(base::File::Error error);
  void DidTryRemoveDirectory(base::File::Error remove_file_error,
                             base::File::Error remove_directory_error);
  void DidRemoveFile(StatusCallback callback, base::File::Error error);

#if DCHECK_IS_ON()
  bool did_run_ = false;
#endif
  FileSystemURL url_;
  StatusCallback callback_;
  base::WeakPtrFactory<RemoveOperationDelegate> weak_factory_{this};
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_REMOVE_OPERATION_DELEGATE_H_
