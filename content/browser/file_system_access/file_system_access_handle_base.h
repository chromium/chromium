// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_HANDLE_BASE_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_HANDLE_BASE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "content/browser/file_system_access/file_system_access_lock_manager.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/file_system_access/file_system_access_transfer_token_impl.h"
#include "content/common/content_export.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_cloud_identifier.mojom.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom.h"
#include "third_party/blink/public/mojom/permissions/permission_status.mojom.h"

namespace storage {
class FileSystemContext;
}  // namespace storage

namespace content {

class WebContents;
class FileSystemAccessSafeMoveHelper;

// Base class for File and Directory handle implementations. Holds data that is
// common to both and (will) deal with functionality that is common as well,
// such as permission requests. Instances of this class should be owned by the
// FileSystemAccessManagerImpl instance passed in to the constructor.
//
// This class is not thread safe, all methods must be called from the same
// sequence. That sequence also has to be the same sequence on which the
// FileSystemAccessPermissionContext expects to be interacted with, which
// is the UI thread.
class CONTENT_EXPORT FileSystemAccessHandleBase {
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
  ~FileSystemAccessHandleBase();

  const storage::FileSystemURL& url() const { return url_; }
  const SharedHandleState& handle_state() const { return handle_state_; }
  const BindingContext& context() const { return context_; }
  FileSystemAccessManagerImpl* manager() { return manager_; }
  storage::FileSystemContext* file_system_context() {
    return manager()->context();
  }

  PermissionStatus GetReadPermissionStatus();
  PermissionStatus GetWritePermissionStatus();
  storage::FileSystemURL GetParentURLForTesting() { return GetParentURL(); }

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

  // TODO(crbug.com/40198034): Implement move and rename for directory handles.
  // Implementation for the Move method in the
  // blink::mojom::FileSystemAccessFileHandle and DirectoryHandle interfaces.
  void DoMove(mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>
                  destination_directory,
              const std::string& new_entry_name,
              bool has_transient_user_activation,
              base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)>
                  callback);
  // Implementation for the Rename method in the
  // blink::mojom::FileSystemAccessFileHandle and DirectoryHandle interfaces.
  void DoRename(const std::string& new_entry_name,
                bool has_transient_user_activation,
                base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)>
                    callback);

  // Implementation for the Remove and RemoveEntry methods in the
  // blink::mojom::FileSystemAccessFileHandle and DirectoryHandle interfaces.
  void DoRemove(const storage::FileSystemURL& url,
                bool recurse,
                base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)>
                    callback);
  void DidTakeRemoveLock(
      const storage::FileSystemURL& url,
      bool recurse,
      base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)> callback,
      scoped_refptr<FileSystemAccessLockManager::LockHandle> lock);

  // Implementation for the GetCloudIdentifiers method in the
  // blink::mojom::FileSystemAccessFileHandle and DirectoryHandle interfaces.
  void DoGetCloudIdentifiers(
      FileSystemAccessPermissionContext::HandleType handle_type,
      ContentBrowserClient::GetCloudIdentifiersCallback callback);

  // Invokes `callback`, possibly after first requesting write permission. If
  // permission isn't granted, `no_permission_callback` is invoked instead. The
  // callbacks can be invoked synchronously.
  template <typename CallbackArgType>
  void RunWithWritePermission(
      base::OnceCallback<void(CallbackArgType)> callback,
      base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr,
                              CallbackArgType)> no_permission_callback,
      CallbackArgType callback_arg);

 protected:
  virtual base::WeakPtr<FileSystemAccessHandleBase> AsWeakPtr() = 0;

  SEQUENCE_CHECKER(sequence_checker_);

 private:
  storage::FileSystemURL GetParentURL();
  void DidRequestPermission(
      bool writable,
      base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr,
                              PermissionStatus)> callback,
      FileSystemAccessPermissionGrant::PermissionRequestOutcome outcome);

  void DidResolveTokenToMove(
      const std::string& new_entry_name,
      bool has_transient_user_activation,
      base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)> callback,
      FileSystemAccessTransferTokenImpl* resolved_token);
  void PrepareForMove(
      storage::FileSystemURL destination_url,
      bool has_write_access_to_destination,
      bool has_transient_user_activation,
      base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)>
          callback);
  void DidTakeMoveLocks(
      storage::FileSystemURL destination_url,
      bool has_transient_user_activation,
      bool has_write_access_to_destination,
      base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)> callback,
      std::vector<scoped_refptr<FileSystemAccessLockManager::LockHandle>>
          locks);
  // Only called if the move operation is not allowed to overwrite the target.
  void ConfirmMoveWillNotOverwriteDestination(
      const storage::FileSystemURL& destination_url,
      std::vector<scoped_refptr<FileSystemAccessLockManager::LockHandle>> locks,
      bool has_transient_user_activation,
      base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)> callback,
      base::File::Error result);
  void DoPerformMoveOperation(
      const storage::FileSystemURL& destination_url,
      std::vector<scoped_refptr<FileSystemAccessLockManager::LockHandle>> locks,
      bool has_transient_user_activation,
      base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)>
          callback);

  void DidMove(
      storage::FileSystemURL destination_url,
      std::vector<scoped_refptr<FileSystemAccessLockManager::LockHandle>> locks,
      std::unique_ptr<FileSystemAccessSafeMoveHelper> move_helper,
      base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)> callback,
      blink::mojom::FileSystemAccessErrorPtr result);

  bool ShouldTrackUsage(const storage::FileSystemURL& url) const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return url.type() != storage::kFileSystemTypeTemporary &&
           url.type() != storage::kFileSystemTypeTest;
  }

  // The FileSystemAccessManagerImpl that owns this instance.
  const raw_ptr<FileSystemAccessManagerImpl, DanglingUntriaged> manager_ =
      nullptr;
  base::WeakPtr<WebContents> web_contents_
      GUARDED_BY_CONTEXT(sequence_checker_);
  const BindingContext context_;
  storage::FileSystemURL url_ GUARDED_BY_CONTEXT(sequence_checker_);
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
