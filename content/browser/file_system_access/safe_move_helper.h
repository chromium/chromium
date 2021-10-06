// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_SAFE_MOVE_HELPER_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_SAFE_MOVE_HELPER_H_

#include "base/bind_post_task.h"
#include "base/files/file.h"
#include "base/sequence_checker.h"
#include "components/download/public/common/quarantine_connection.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/common/content_export.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"

namespace content {

// TODO(crbug.com/1250534): Support safely moving directories. For now, this
// class only supports moving files. Moving directories will require running
// safe browsing checks on all files before moving.
//
// Helper class which moves files (and eventually directories). Safe browsing
// checks are performed and the mark of the web is added for certain file system
// types, as appropriate.
class CONTENT_EXPORT SafeMoveHelper {
 public:
  using SafeMoveHelperCallback =
      base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)>;

  SafeMoveHelper(
      base::WeakPtr<FileSystemAccessManagerImpl> manager,
      const FileSystemAccessManagerImpl::BindingContext& context,
      const storage::FileSystemURL& source_url,
      const storage::FileSystemURL& dest_url,
      storage::FileSystemOperation::CopyOrMoveOption option,
      download::QuarantineConnectionCallback quarantine_connection_callback,
      bool has_transient_user_activation);
  SafeMoveHelper(const SafeMoveHelper&) = delete;
  SafeMoveHelper& operator=(const SafeMoveHelper&) = delete;
  ~SafeMoveHelper();

  void Start(SafeMoveHelperCallback callback);

  const storage::FileSystemURL& source_url() const { return source_url_; }
  const storage::FileSystemURL& dest_url() const { return dest_url_; }

  using HashCallback = base::OnceCallback<
      void(base::File::Error error, const std::string& hash, int64_t size)>;
  void ComputeHashForSourceFileForTesting(HashCallback callback) {
    ComputeHashForSourceFile(std::move(callback));
  }

 private:
  // TODO(asully): This code is copied from FSAHandleBase and now has 3 copies
  // within this directory. Move to a helper.
  //
  // Invokes `method` on the correct sequence on this handle's
  // FileSystemOperationRunner, passing `args` and a callback to the method. The
  // passed in `callback` is wrapped to make sure it is called on the correct
  // sequence before passing it off to the `method`.
  //
  // Note that `callback` is passed to this method before other arguments, while
  // the wrapped callback will be passed as last argument to the underlying
  // FileSystemOperation `method`.
  template <typename... MethodArgs,
            typename... ArgsMinusCallback,
            typename... CallbackArgs>
  void DoFileSystemOperation(
      const base::Location& from_here,
      storage::FileSystemOperationRunner::OperationID (
          storage::FileSystemOperationRunner::*method)(MethodArgs...),
      base::OnceCallback<void(CallbackArgs...)> callback,
      ArgsMinusCallback&&... args) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // Wrap the passed in callback in one that posts a task back to the current
    // sequence.
    auto wrapped_callback = base::BindPostTask(
        base::SequencedTaskRunnerHandle::Get(), std::move(callback));

    // And then post a task to the sequence bound operation runner to run the
    // provided method with the provided arguments (and the wrapped callback).
    //
    // FileSystemOperationRunner assumes file_system_context() is kept alive, to
    // make sure this happens it is bound to a callback that otherwise does
    // nothing.
    manager_->operation_runner()
        .AsyncCall(base::IgnoreResult(method))
        .WithArgs(std::forward<ArgsMinusCallback>(args)...,
                  std::move(wrapped_callback))
        .Then(base::BindOnce([](scoped_refptr<storage::FileSystemContext>) {},
                             base::WrapRefCounted(manager_->context())));
  }

  SEQUENCE_CHECKER(sequence_checker_);

  void DoAfterWriteCheck(base::File::Error hash_result,
                         const std::string& hash,
                         int64_t size);
  void DidAfterWriteCheck(
      FileSystemAccessPermissionContext::AfterWriteCheckResult result);
  void DidFileSkipQuarantine(base::File::Error result);
  void DidFileDoQuarantine(
      const storage::FileSystemURL& target_url,
      const GURL& referrer_url,
      mojo::Remote<quarantine::mojom::Quarantine> quarantine_remote,
      base::File::Error result);
  void DidAnnotateFile(
      mojo::Remote<quarantine::mojom::Quarantine> quarantine_remote,
      quarantine::mojom::QuarantineFileResult result);

  void ComputeHashForSourceFile(HashCallback callback);

  // After write and quarantine checks should apply to paths on all filesystems
  // except temporary file systems.
  // TOOD(crbug.com/1103076): Extend this check to non-native paths.
  bool RequireSecurityChecks() const {
    return dest_url().type() != storage::kFileSystemTypeTemporary;
  }

  base::WeakPtr<FileSystemAccessManagerImpl> manager_;
  FileSystemAccessManagerImpl::BindingContext context_;

  const storage::FileSystemURL source_url_;
  const storage::FileSystemURL dest_url_;

  storage::FileSystemOperation::CopyOrMoveOption option_;

  download::QuarantineConnectionCallback quarantine_connection_callback_;

  bool has_transient_user_activation_ = false;

  SafeMoveHelperCallback callback_;

  base::WeakPtrFactory<SafeMoveHelper> weak_factory_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_SAFE_MOVE_HELPER_H_
