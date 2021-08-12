// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_HANDLE_BASE_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_HANDLE_BASE_H_

#include "base/bind_post_task.h"
#include "base/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/isolated_context.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace storage {
class FileSystemContext;
class FileSystemOperationRunner;
}  // namespace storage

namespace content {

class WebContentsImpl;

// Base class for File and Directory handle implementations. Holds data that is
// common to both and (will) deal with functionality that is common as well,
// such as permission requests. Instances of this class should be owned by the
// FileSystemAccessManagerImpl instance passed in to the constructor.
//
// This class is not thread safe, all methods must be called from the same
// sequence. That sequence also has to be the same sequence on which the
// FileSystemAccessPermissionContext expects to be interacted with, which
// is the UI thread.
class CONTENT_EXPORT FileSystemAccessHandleBase : public WebContentsObserver {
 public:
  using BindingContext = FileSystemAccessManagerImpl::BindingContext;
  using SharedHandleState = FileSystemAccessManagerImpl::SharedHandleState;
  using PermissionStatus = blink::mojom::PermissionStatus;

  FileSystemAccessHandleBase(FileSystemAccessManagerImpl* manager,
                             const BindingContext& context,
                             const storage::FileSystemURL& url,
                             const SharedHandleState& handle_state);
  FileSystemAccessHandleBase(const FileSystemAccessHandleBase&) = delete;
  FileSystemAccessHandleBase& operator=(const FileSystemAccessHandleBase&) =
      delete;
  ~FileSystemAccessHandleBase() override;

  const storage::FileSystemURL& url() const { return url_; }
  const SharedHandleState& handle_state() const { return handle_state_; }
  const BindingContext& context() const { return context_; }

  PermissionStatus GetReadPermissionStatus();
  PermissionStatus GetWritePermissionStatus();

  // Implementation for the GetPermissionStatus method in the
  // blink::mojom::FileSystemAccessFileHandle and DirectoryHandle interfaces.
  void DoGetPermissionStatus(
      bool writable,
      base::OnceCallback<void(PermissionStatus)> callback);
  // Implementation for the RequestPermission method in the
  // blink::mojom::FileSystemAccessFileHandle and DirectoryHandle interfaces.
  void DoRequestPermission(
      bool writable,
      base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr,
                              PermissionStatus)> callback);

  // Implementation for the Remove and RemoveEntry methods in the
  // blink::mojom::FileSystemAccessFileHandle and DirectoryHandle interfaces.
  void DoRemove(const storage::FileSystemURL& url,
                bool recurse,
                base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)>
                    callback);

  // Invokes |callback|, possibly after first requesting write permission. If
  // permission isn't granted, |permission_denied| is invoked instead. The
  // callbacks can be invoked synchronously.
  template <typename CallbackArgType>
  void RunWithWritePermission(
      base::OnceCallback<void(CallbackArgType)> callback,
      base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr,
                              CallbackArgType)> no_permission_callback,
      CallbackArgType callback_arg);

 protected:
  FileSystemAccessManagerImpl* manager() { return manager_; }
  storage::FileSystemContext* file_system_context() {
    return manager()->context();
  }

  WebContentsImpl* web_contents() const;

  virtual base::WeakPtr<FileSystemAccessHandleBase> AsWeakPtr() = 0;

  // Invokes |method| on the correct sequence on this handle's
  // FileSystemOperationRunner, passing |args| and a callback to the method. The
  // passed in |callback| is wrapped to make sure it is called on the correct
  // sequence before passing it off to the |method|.
  //
  // Note that |callback| is passed to this method before other arguments, while
  // the wrapped callback will be passed as last argument to the underlying
  // FileSystemOperation |method|.
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
    // make sure this happens it is bound to a DoNothing callback.
    manager()
        ->operation_runner()
        .AsyncCall(base::IgnoreResult(method))
        .WithArgs(std::forward<ArgsMinusCallback>(args)...,
                  std::move(wrapped_callback))
        .Then(base::BindOnce(
            base::DoNothing::Once<scoped_refptr<storage::FileSystemContext>>(),
            base::WrapRefCounted(file_system_context())));
  }
  // Same as the previous overload, but using RepeatingCallback and
  // BindRepeating instead.
  template <typename... MethodArgs,
            typename... ArgsMinusCallback,
            typename... CallbackArgs>
  void DoFileSystemOperation(
      const base::Location& from_here,
      storage::FileSystemOperationRunner::OperationID (
          storage::FileSystemOperationRunner::*method)(MethodArgs...),
      base::RepeatingCallback<void(CallbackArgs...)> callback,
      ArgsMinusCallback&&... args) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    // Wrap the passed in callback in one that posts a task back to the current
    // sequence.
    auto wrapped_callback = base::BindRepeating(
        [](scoped_refptr<base::SequencedTaskRunner> runner,
           const base::RepeatingCallback<void(CallbackArgs...)>& callback,
           CallbackArgs... args) {
          runner->PostTask(
              FROM_HERE,
              base::BindOnce(callback, std::forward<CallbackArgs>(args)...));
        },
        base::SequencedTaskRunnerHandle::Get(), std::move(callback));

    // And then post a task to the sequence bound operation runner to run the
    // provided method with the provided arguments (and the wrapped callback).
    //
    // FileSystemOperationRunner assumes file_system_context() is kept alive, to
    // make sure this happens it is bound to a DoNothing callback.
    manager()
        ->operation_runner()
        .AsyncCall(base::IgnoreResult(method))
        .WithArgs(std::forward<ArgsMinusCallback>(args)...,
                  std::move(wrapped_callback))
        .Then(base::BindOnce(
            base::DoNothing::Once<scoped_refptr<storage::FileSystemContext>>(),
            base::WrapRefCounted(file_system_context())));
  }

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  void DidRequestPermission(
      bool writable,
      base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr,
                              PermissionStatus)> callback,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome outcome);

  bool ShouldTrackUsage() const {
    return url_.type() != storage::kFileSystemTypeTemporary &&
           url_.type() != storage::kFileSystemTypeTest;
  }

  // The FileSystemAccessManagerImpl that owns this instance.
  FileSystemAccessManagerImpl* const manager_;
  const BindingContext context_;
  const storage::FileSystemURL url_;
  const SharedHandleState handle_state_;
};

template <typename CallbackArgType>
void FileSystemAccessHandleBase::RunWithWritePermission(
    base::OnceCallback<void(CallbackArgType)> callback,
    base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr,
                            CallbackArgType)> no_permission_callback,
    CallbackArgType callback_arg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DoRequestPermission(
      /*writable=*/true,
      base::BindOnce(
          [](base::OnceCallback<void(CallbackArgType)> callback,
             base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr,
                                     CallbackArgType)> no_permission_callback,
             CallbackArgType callback_arg,
             blink::mojom::FileSystemAccessErrorPtr result,
             blink::mojom::PermissionStatus status) {
            if (status == blink::mojom::PermissionStatus::GRANTED) {
              std::move(callback).Run(std::move(callback_arg));
              return;
            }
            if (result->status == blink::mojom::FileSystemAccessStatus::kOk) {
              result->status =
                  blink::mojom::FileSystemAccessStatus::kPermissionDenied;
            }
            std::move(no_permission_callback)
                .Run(std::move(result), std::move(callback_arg));
          },
          std::move(callback), std::move(no_permission_callback),
          std::move(callback_arg)));
}

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_HANDLE_BASE_H_
