// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_HANDLE_BASE_H_
#define CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_HANDLE_BASE_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
#include "content/browser/native_file_system/native_file_system_manager_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/web_contents_observer.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/isolated_context.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace storage {
class FileSystemContext;
class FileSystemOperationRunner;
}  // namespace storage

namespace content {

// Base class for File and Directory handle implementations. Holds data that is
// common to both and (will) deal with functionality that is common as well,
// such as permission requests. Instances of this class should be owned by the
// NativeFileSystemManagerImpl instance passed in to the constructor.
//
// This class is not thread safe, all methods must be called from the same
// sequence. That sequence also has to be the same sequence on which the
// NativeFileSystemPermissionContext expects to be interacted with, which
// is the UI thread.
class CONTENT_EXPORT NativeFileSystemHandleBase
    : public NativeFileSystemPermissionGrant::Observer,
      public WebContentsObserver {
 public:
  using BindingContext = NativeFileSystemManagerImpl::BindingContext;
  using SharedHandleState = NativeFileSystemManagerImpl::SharedHandleState;
  using PermissionStatus = blink::mojom::PermissionStatus;

  NativeFileSystemHandleBase(NativeFileSystemManagerImpl* manager,
                             const BindingContext& context,
                             const storage::FileSystemURL& url,
                             const SharedHandleState& handle_state,
                             bool is_directory);
  ~NativeFileSystemHandleBase() override;

  const storage::FileSystemURL& url() const { return url_; }
  const SharedHandleState& handle_state() const { return handle_state_; }
  const storage::IsolatedContext::ScopedFSHandle& file_system() const {
    return handle_state_.file_system;
  }

  PermissionStatus GetReadPermissionStatus();
  PermissionStatus GetWritePermissionStatus();

  // Implementation for the GetPermissionStatus method in the
  // blink::mojom::NativeFileSystemFileHandle and DirectoryHandle interfaces.
  void DoGetPermissionStatus(
      bool writable,
      base::OnceCallback<void(PermissionStatus)> callback);
  // Implementation for the RequestPermission method in the
  // blink::mojom::NativeFileSystemFileHandle and DirectoryHandle interfaces.
  void DoRequestPermission(
      bool writable,
      base::OnceCallback<void(blink::mojom::NativeFileSystemErrorPtr,
                              PermissionStatus)> callback);

  // Invokes |callback|, possibly after first requesting write permission. If
  // permission isn't granted, |permission_denied| is invoked instead. The
  // callbacks can be invoked synchronously.
  template <typename CallbackArgType>
  void RunWithWritePermission(
      base::OnceCallback<void(CallbackArgType)> callback,
      base::OnceCallback<void(CallbackArgType)> no_permission_callback,
      CallbackArgType callback_arg);

 protected:
  NativeFileSystemManagerImpl* manager() { return manager_; }
  const BindingContext& context() { return context_; }
  storage::FileSystemContext* file_system_context() {
    return manager()->context();
  }

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(WebContentsObserver::web_contents());
  }

  virtual base::WeakPtr<NativeFileSystemHandleBase> AsWeakPtr() = 0;

  // NativeFileSystemPermissionGrant::Observer:
  void OnPermissionStatusChanged() override;

  // Invokes |method| on the correct sequence on this handle's
  // FileSystemOperationRunner, passing |args| and a callback to the method. The
  // passed in |callback| is wrapped to make sure it is called on the correct
  // sequence before passing it off to the |method|.
  //
  // Note that |callback| is passed to this method before other arguments, while
  // the wrapped callback will be passed as last argument to the underlying
  // FileSystemOperation |method|.
  //
  // TODO(mek): Once Promises are a thing, this can be done a lot cleaner, and
  // mostly just be integrated in base::SequenceBound, eliminating the need for
  // these helper methods.
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
    auto wrapped_callback = base::BindOnce(
        [](scoped_refptr<base::SequencedTaskRunner> runner,
           base::OnceCallback<void(CallbackArgs...)> callback,
           CallbackArgs... args) {
          runner->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback),
                                          std::forward<CallbackArgs>(args)...));
        },
        base::SequencedTaskRunnerHandle::Get(), std::move(callback));

    // And then post a task to the sequence bound operation runner to run the
    // provided method with the provided arguments (and the wrapped callback).
    manager()->operation_runner().PostTaskWithThisObject(
        from_here,
        base::BindOnce(
            [](scoped_refptr<storage::FileSystemContext>,
               storage::FileSystemOperationRunner::OperationID (
                   storage::FileSystemOperationRunner::*method)(MethodArgs...),
               MethodArgs... args, storage::FileSystemOperationRunner* runner) {
              (runner->*method)(std::forward<MethodArgs>(args)...);
            },
            base::WrapRefCounted(file_system_context()), method,
            std::forward<ArgsMinusCallback>(args)...,
            std::move(wrapped_callback)));
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
    manager()->operation_runner().PostTaskWithThisObject(
        from_here,
        base::BindOnce(
            [](scoped_refptr<storage::FileSystemContext>,
               storage::FileSystemOperationRunner::OperationID (
                   storage::FileSystemOperationRunner::*method)(MethodArgs...),
               MethodArgs... args, storage::FileSystemOperationRunner* runner) {
              (runner->*method)(std::forward<MethodArgs>(args)...);
            },
            base::WrapRefCounted(file_system_context()), method,
            std::forward<ArgsMinusCallback>(args)...,
            std::move(wrapped_callback)));
  }

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  void DidRequestPermission(
      bool writable,
      base::OnceCallback<void(blink::mojom::NativeFileSystemErrorPtr,
                              PermissionStatus)> callback,
      NativeFileSystemPermissionGrant::PermissionRequestOutcome outcome);

  bool ShouldTrackUsage() const {
    return url_.type() == storage::kFileSystemTypeNativeLocal;
  }

  // The NativeFileSystemManagerImpl that owns this instance.
  NativeFileSystemManagerImpl* const manager_;
  const BindingContext context_;
  const storage::FileSystemURL url_;
  const SharedHandleState handle_state_;

  base::FilePath directory_for_usage_tracking_;
  bool was_readable_at_last_check_ = false;
  bool was_writable_at_last_check_ = false;

  void UpdateUsage();

  DISALLOW_COPY_AND_ASSIGN(NativeFileSystemHandleBase);
};

template <typename CallbackArgType>
void NativeFileSystemHandleBase::RunWithWritePermission(
    base::OnceCallback<void(CallbackArgType)> callback,
    base::OnceCallback<void(CallbackArgType)> no_permission_callback,
    CallbackArgType callback_arg) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DoRequestPermission(
      /*writable=*/true,
      base::BindOnce(
          [](base::OnceCallback<void(CallbackArgType)> callback,
             base::OnceCallback<void(CallbackArgType)> no_permission_callback,
             CallbackArgType callback_arg,
             blink::mojom::NativeFileSystemErrorPtr result,
             blink::mojom::PermissionStatus status) {
            if (status == blink::mojom::PermissionStatus::GRANTED) {
              std::move(callback).Run(std::move(callback_arg));
              return;
            }
            std::move(no_permission_callback).Run(std::move(callback_arg));
          },
          std::move(callback), std::move(no_permission_callback),
          std::move(callback_arg)));
}

}  // namespace content

#endif  // CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_HANDLE_BASE_H_
