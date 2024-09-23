// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef STORAGE_BROWSER_FILE_SYSTEM_RECURSIVE_OPERATION_DELEGATE_H_
#define STORAGE_BROWSER_FILE_SYSTEM_RECURSIVE_OPERATION_DELEGATE_H_

#include "base/component_export.h"
#include "base/containers/queue.h"
#include "base/containers/stack.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_url.h"

namespace storage {

class FileSystemContext;
class FileSystemOperationRunner;

// A base class for recursive operation delegates.
//
// In short, each subclass should override ProcessFile and ProcessDirectory
// to process a directory or a file. To start the recursive operation it
// should also call StartRecursiveOperation.
class COMPONENT_EXPORT(STORAGE_BROWSER) RecursiveOperationDelegate {
 public:
  using StatusCallback = FileSystemOperation::StatusCallback;
  using FileEntryList = FileSystemOperation::FileEntryList;
  using ErrorBehavior = FileSystemOperation::ErrorBehavior;

  RecursiveOperationDelegate(const RecursiveOperationDelegate&) = delete;
  RecursiveOperationDelegate& operator=(const RecursiveOperationDelegate&) =
      delete;

  virtual ~RecursiveOperationDelegate();

  // This is called when the consumer of this instance starts a non-recursive
  // operation.
  virtual void Run() = 0;

  // This is called when the consumer of this instance starts a recursive
  // operation.
  virtual void RunRecursively() = 0;

  // This is called each time a file is found while recursively
  // performing an operation.
  virtual void ProcessFile(const FileSystemURL& url,
                           StatusCallback callback) = 0;

  // This is called each time a directory is found while recursively
  // performing an operation.
  virtual void ProcessDirectory(const FileSystemURL& url,
                                StatusCallback callback) = 0;

  // This is called each time after files and subdirectories for a
  // directory is processed while recursively performing an operation.
  virtual void PostProcessDirectory(const FileSystemURL& url,
                                    StatusCallback callback) = 0;

  // Cancels the currently running operation.
  void Cancel();

  // This should be implemented by the outermost concrete class.
  virtual base::WeakPtr<RecursiveOperationDelegate> AsWeakPtr() = 0;

 protected:
  explicit RecursiveOperationDelegate(FileSystemContext* file_system_context);

  // Starts to process files/directories recursively from the given `root`.
  // This will call ProcessFile and ProcessDirectory on each file or directory.
  //
  // First, this tries to call ProcessFile with `root` regardless whether it is
  // actually a file or a directory. If it is a directory, ProcessFile should
  // return File::FILE_NOT_A_FILE.
  //
  // For each directory, the recursive operation works as follows:
  // ProcessDirectory is called first for the directory.
  // Then the directory contents are read (to obtain its sub directories and
  // files in it).
  // ProcessFile is called for found files.
  // The same step is recursively applied to each subdirectory.
  // After all files and subdirectories in a directory are processed,
  // PostProcessDirectory is called for the directory.
  // Here is an example;
  // a_dir/ -+- b1_dir/ -+- c1_dir/ -+- d1_file
  //         |           |           |
  //         |           +- c2_file  +- d2_file
  //         |
  //         +- b2_dir/ --- e_dir/
  //         |
  //         +- b3_file
  //         |
  //         +- b4_file
  // Then traverse order is:
  // ProcessFile(a_dir) (This should return File::FILE_NOT_A_FILE).
  // ProcessDirectory(a_dir).
  // ProcessFile(b3_file).
  // ProcessFile(b4_file).
  // ProcessDirectory(b1_dir).
  // ProcessFile(c2_file)
  // ProcessDirectory(c1_dir).
  // ProcessFile(d1_file).
  // ProcessFile(d2_file).
  // PostProcessDirectory(c1_dir)
  // PostProcessDirectory(b1_dir).
  // ProcessDirectory(b2_dir)
  // ProcessDirectory(e_dir)
  // PostProcessDirectory(e_dir)
  // PostProcessDirectory(b2_dir)
  // PostProcessDirectory(a_dir)
  //
  // `error_behavior` is to specify how this behaves when an operation have
  // failed.
  // `callback` is fired with `base::File::FILE_OK` when every file/directory
  // under `root` is processed, or fired earlier when any suboperation fails.
  //
  // For `ERROR_BEHAVIOR_SKIP`, `callback` is fired at the end of the operation
  // or when an unrecoverable error occurs. `callback` is called with the last
  // error or `base::File::FILE_OK` if no error occurred.
  void StartRecursiveOperation(const FileSystemURL& root,
                               ErrorBehavior error_behavior,
                               StatusCallback callback);

  FileSystemContext* file_system_context() { return file_system_context_; }
  const FileSystemContext* file_system_context() const {
    return file_system_context_;
  }

  FileSystemOperationRunner* operation_runner();

  // Called when Cancel() is called. This is a hook to do something more
  // in a derived class. By default, do nothing.
  virtual void OnCancel();

 private:
  void TryProcessFile(const FileSystemURL& root);
  void DidTryProcessFile(const FileSystemURL& root, base::File::Error error);
  void ProcessNextDirectory();
  void DidProcessDirectory(base::File::Error error);
  void DidReadDirectory(const FileSystemURL& parent,
                        base::File::Error error,
                        FileEntryList entries,
                        bool has_more);
  void ProcessPendingFiles();
  void DidProcessFile(const FileSystemURL& url, base::File::Error error);
  void ProcessSubDirectory();
  void DidPostProcessDirectory(base::File::Error error);
  void SetPreviousError(base::File::Error error);

  // Called when all recursive operation is done (or an error occurs).
  void Done(base::File::Error error);

  raw_ptr<FileSystemContext, DanglingUntriaged> file_system_context_;
  StatusCallback callback_;
  base::stack<FileSystemURL> pending_directories_;
  base::stack<base::queue<FileSystemURL>> pending_directory_stack_;
  base::queue<FileSystemURL> pending_files_;
  bool canceled_ = false;
  ErrorBehavior error_behavior_ = ErrorBehavior::ERROR_BEHAVIOR_ABORT;
  base::File::Error previous_error_ = base::File::FILE_OK;
};

}  // namespace storage

#endif  // STORAGE_BROWSER_FILE_SYSTEM_RECURSIVE_OPERATION_DELEGATE_H_
