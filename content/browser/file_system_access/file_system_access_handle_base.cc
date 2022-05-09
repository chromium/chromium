// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_handle_base.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "content/browser/file_system_access/file_system_access_directory_handle_impl.h"
#include "content/browser/file_system_access/file_system_access_error.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/file_system_access/file_system_access_transfer_token_impl.h"
#include "content/browser/file_system_access/safe_move_helper.h"
#include "content/browser/renderer_host/back_forward_cache_disable.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "net/base/filename_util.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom-forward.h"

namespace content {

using WriteLock = FileSystemAccessWriteLockManager::WriteLock;
using WriteLockType = FileSystemAccessWriteLockManager::WriteLockType;

namespace {
std::string GetURLDisplayName(const storage::FileSystemURL& url) {
  return base::UTF16ToUTF8(url.path().BaseName().LossyDisplayName());
}

}  // namespace

FileSystemAccessHandleBase::FileSystemAccessHandleBase(
    FileSystemAccessManagerImpl* manager,
    const BindingContext& context,
    const storage::FileSystemURL& url,
    const SharedHandleState& handle_state)
    : manager_(manager),
      context_(context),
      url_(url),
      handle_state_(handle_state) {
  DCHECK(manager_);

  // We support sandboxed file system and local file systems on all platforms.
  DCHECK(url_.type() == storage::kFileSystemTypeLocal ||
         url_.type() == storage::kFileSystemTypeTemporary ||
         url_.mount_type() == storage::kFileSystemTypeExternal ||
         url_.type() == storage::kFileSystemTypeTest)
      << url_.type();

  if (ShouldTrackUsage()) {
    DCHECK(url_.mount_type() == storage::kFileSystemTypeLocal ||
           url_.mount_type() == storage::kFileSystemTypeExternal)
        << url_.mount_type();

    WebContents* web_contents =
        WebContentsImpl::FromRenderFrameHostID(context_.frame_id);
    if (web_contents) {
      web_contents_ = web_contents->GetWeakPtr();
    }

    // Disable back-forward cache as File System Access's usage of
    // RenderFrameHost::IsActive at the moment is not compatible with bfcache.
    BackForwardCache::DisableForRenderFrameHost(
        context_.frame_id,
        BackForwardCacheDisable::DisabledReason(
            BackForwardCacheDisable::DisabledReasonId::kFileSystemAccess));
    if (web_contents_) {
      static_cast<WebContentsImpl*>(web_contents_.get())
          ->IncrementFileSystemAccessHandleCount();
    }
  }
}

FileSystemAccessHandleBase::~FileSystemAccessHandleBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (ShouldTrackUsage() && web_contents_) {
    static_cast<WebContentsImpl*>(web_contents_.get())
        ->DecrementFileSystemAccessHandleCount();
  }
}

FileSystemAccessHandleBase::PermissionStatus
FileSystemAccessHandleBase::GetReadPermissionStatus() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return handle_state_.read_grant->GetStatus();
}

FileSystemAccessHandleBase::PermissionStatus
FileSystemAccessHandleBase::GetWritePermissionStatus() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // It is not currently possible to have write only handles, so first check the
  // read permission status. See also:
  // http://wicg.github.io/file-system-access/#api-filesystemhandle-querypermission
  PermissionStatus read_status = GetReadPermissionStatus();
  if (read_status != PermissionStatus::GRANTED)
    return read_status;

  return handle_state_.write_grant->GetStatus();
}

void FileSystemAccessHandleBase::DoGetPermissionStatus(
    bool writable,
    base::OnceCallback<void(PermissionStatus)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(writable ? GetWritePermissionStatus()
                                   : GetReadPermissionStatus());
}

void FileSystemAccessHandleBase::DoRequestPermission(
    bool writable,
    base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr,
                            PermissionStatus)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PermissionStatus current_status =
      writable ? GetWritePermissionStatus() : GetReadPermissionStatus();
  // If we already have a valid permission status, just return that. Also just
  // return the current permission status if this is called from a worker, as we
  // don't support prompting for increased permissions from workers.
  //
  // Currently the worker check here is redundant because there is no way for
  // workers to get File System Access handles. While workers will never be able
  // to call chooseEntries(), they will be able to receive existing handles from
  // windows via postMessage() and IndexedDB.
  if (current_status != PermissionStatus::ASK || context_.is_worker()) {
    std::move(callback).Run(file_system_access_error::Ok(), current_status);
    return;
  }
  if (!writable) {
    handle_state_.read_grant->RequestPermission(
        context().frame_id,
        FileSystemAccessPermissionGrant::UserActivationState::kRequired,
        base::BindOnce(&FileSystemAccessHandleBase::DidRequestPermission,
                       AsWeakPtr(), writable, std::move(callback)));
    return;
  }

  // Ask for both read and write permission at the same time, the permission
  // context should coalesce these into one prompt.
  if (GetReadPermissionStatus() == PermissionStatus::ASK) {
    // Ignore callback for the read permission request; if the request fails,
    // the write permission request probably fails the same way. And we check
    // the final permission status after the permission request completes
    // anyway.
    handle_state_.read_grant->RequestPermission(
        context().frame_id,
        FileSystemAccessPermissionGrant::UserActivationState::kRequired,
        base::DoNothing());
  }

  handle_state_.write_grant->RequestPermission(
      context().frame_id,
      FileSystemAccessPermissionGrant::UserActivationState::kRequired,
      base::BindOnce(&FileSystemAccessHandleBase::DidRequestPermission,
                     AsWeakPtr(), writable, std::move(callback)));
}

void FileSystemAccessHandleBase::DidRequestPermission(
    bool writable,
    base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr,
                            PermissionStatus)> callback,
    FileSystemAccessPermissionGrant::PermissionRequestOutcome outcome) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  using Outcome = FileSystemAccessPermissionGrant::PermissionRequestOutcome;
  switch (outcome) {
    case Outcome::kInvalidFrame:
    case Outcome::kThirdPartyContext:
      std::move(callback).Run(
          file_system_access_error::FromStatus(
              blink::mojom::FileSystemAccessStatus::kSecurityError,
              "Not allowed to request permissions in this context."),
          writable ? GetWritePermissionStatus() : GetReadPermissionStatus());
      return;
    case Outcome::kNoUserActivation:
      std::move(callback).Run(
          file_system_access_error::FromStatus(
              blink::mojom::FileSystemAccessStatus::kSecurityError,
              "User activation is required to request permissions."),
          writable ? GetWritePermissionStatus() : GetReadPermissionStatus());
      return;
    case Outcome::kBlockedByContentSetting:
    case Outcome::kUserGranted:
    case Outcome::kUserDenied:
    case Outcome::kUserDismissed:
    case Outcome::kRequestAborted:
    case Outcome::kGrantedByContentSetting:
    case Outcome::kGrantedByPersistentPermission:
    case Outcome::kGrantedByAncestorPersistentPermission:
      std::move(callback).Run(
          file_system_access_error::Ok(),
          writable ? GetWritePermissionStatus() : GetReadPermissionStatus());
      return;
  }
  NOTREACHED();
}

void FileSystemAccessHandleBase::DoMove(
    mojo::PendingRemote<blink::mojom::FileSystemAccessTransferToken>
        destination_directory,
    const std::string& new_entry_name,
    bool has_transient_user_activation,
    base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  // TODO(crbug.com/1247850): Allow moves of files outside of the OPFS.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExperimentalWebPlatformFeatures)) {
    if (url().type() != storage::FileSystemType::kFileSystemTypeTemporary) {
      std::move(callback).Run(file_system_access_error::FromStatus(
          blink::mojom::FileSystemAccessStatus::kOperationAborted));
      return;
    }
  }

  if (!FileSystemAccessDirectoryHandleImpl::IsSafePathComponent(
          new_entry_name)) {
    std::move(callback).Run(file_system_access_error::FromStatus(
        blink::mojom::FileSystemAccessStatus::kInvalidArgument));
    return;
  }

  manager()->ResolveTransferToken(
      std::move(destination_directory),
      base::BindOnce(&FileSystemAccessHandleBase::DidResolveTokenToMove,
                     AsWeakPtr(), new_entry_name, has_transient_user_activation,
                     std::move(callback)));
}

void FileSystemAccessHandleBase::DoRename(
    const std::string& new_entry_name,
    bool has_transient_user_activation,
    base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  // TODO(crbug.com/1247850): Allow moves of files outside of the OPFS.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExperimentalWebPlatformFeatures)) {
    if (url().type() != storage::FileSystemType::kFileSystemTypeTemporary) {
      std::move(callback).Run(file_system_access_error::FromStatus(
          blink::mojom::FileSystemAccessStatus::kOperationAborted));
      return;
    }
  }

  if (!FileSystemAccessDirectoryHandleImpl::IsSafePathComponent(
          new_entry_name)) {
    std::move(callback).Run(file_system_access_error::FromStatus(
        blink::mojom::FileSystemAccessStatus::kInvalidArgument));
    return;
  }

  auto dest_parent_url = GetParentURL();
  auto dir_handle = std::make_unique<FileSystemAccessDirectoryHandleImpl>(
      manager(), context(), dest_parent_url, handle_state_);

  DidCreateDestinationDirectoryHandle(new_entry_name, std::move(dir_handle),
                                      has_transient_user_activation,
                                      std::move(callback));
}

void FileSystemAccessHandleBase::DidResolveTokenToMove(
    const std::string& new_entry_name,
    bool has_transient_user_activation,
    base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)> callback,
    FileSystemAccessTransferTokenImpl* resolved_destination_directory) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!resolved_destination_directory) {
    std::move(callback).Run(file_system_access_error::FromStatus(
        blink::mojom::FileSystemAccessStatus::kInvalidArgument));
    return;
  }

  if (resolved_destination_directory->type() !=
      FileSystemAccessPermissionContext::HandleType::kDirectory) {
    mojo::ReportBadMessage(
        "FileSystemHandle::move() was passed a token which is not a directory");
    std::move(callback).Run(file_system_access_error::FromStatus(
        blink::mojom::FileSystemAccessStatus::kInvalidArgument));
    return;
  }

  std::unique_ptr<FileSystemAccessDirectoryHandleImpl> dir_handle =
      resolved_destination_directory->CreateDirectoryHandle(context_);

  DidCreateDestinationDirectoryHandle(new_entry_name, std::move(dir_handle),
                                      has_transient_user_activation,
                                      std::move(callback));
}

void FileSystemAccessHandleBase::DidCreateDestinationDirectoryHandle(
    const std::string& new_entry_name,
    std::unique_ptr<FileSystemAccessDirectoryHandleImpl> dir_handle,
    bool has_transient_user_activation,
    base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Must have write access to the target directory.
  if (dir_handle->GetWritePermissionStatus() !=
      blink::mojom::PermissionStatus::GRANTED) {
    std::move(callback).Run(file_system_access_error::FromStatus(
        blink::mojom::FileSystemAccessStatus::kPermissionDenied));
    return;
  }

  storage::FileSystemURL dest_url;
  blink::mojom::FileSystemAccessErrorPtr error =
      dir_handle->GetChildURL(new_entry_name, &dest_url);
  if (error != file_system_access_error::Ok()) {
    std::move(callback).Run(std::move(error));
    return;
  }

  // TODO(crbug.com/1247850): Allow moves of files outside of the OPFS.
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableExperimentalWebPlatformFeatures)) {
    if (dest_url.type() != storage::FileSystemType::kFileSystemTypeTemporary) {
      std::move(callback).Run(file_system_access_error::FromStatus(
          blink::mojom::FileSystemAccessStatus::kOperationAborted));
      return;
    }
  }

  // The file can only be moved if we can acquire exclusive write locks to
  // both the source or destination URLs.
  std::vector<scoped_refptr<WriteLock>> locks;
  auto source_write_lock =
      manager()->TakeWriteLock(url(), WriteLockType::kExclusive);
  if (!source_write_lock.has_value()) {
    std::move(callback).Run(file_system_access_error::FromStatus(
        blink::mojom::FileSystemAccessStatus::kNoModificationAllowedError,
        base::StrCat(
            {"Failed to move ", GetURLDisplayName(url()),
             ". A FileSystemHandle cannot be moved while it is locked."})));
    return;
  }
  locks.emplace_back(std::move(source_write_lock.value()));

  // Since we're using exclusive locks, we should only acquire the
  // lock of the destination URL if it is different from the source URL.
  if (url() != dest_url) {
    auto dest_write_lock =
        manager()->TakeWriteLock(dest_url, WriteLockType::kExclusive);
    if (!dest_write_lock.has_value()) {
      std::move(callback).Run(file_system_access_error::FromStatus(
          blink::mojom::FileSystemAccessStatus::kNoModificationAllowedError,
          base::StrCat({"Failed to move ", GetURLDisplayName(url()), " to ",
                        GetURLDisplayName(dest_url),
                        ". A FileSystemHandle cannot be moved to a destination "
                        "which is locked."})));
      return;
    }
    locks.emplace_back(std::move(dest_write_lock.value()));
  }

  auto safe_move_helper = std::make_unique<SafeMoveHelper>(
      manager()->AsWeakPtr(), context(), url(), dest_url,
      storage::FileSystemOperation::CopyOrMoveOptionSet(),
      GetContentClient()->browser()->GetQuarantineConnectionCallback(),
      has_transient_user_activation);
  // Allows the unique pointer to be bound to the callback so the helper stays
  // alive until the operation completes.
  SafeMoveHelper* raw_helper = safe_move_helper.get();
  raw_helper->Start(base::BindOnce(
      [](base::WeakPtr<FileSystemAccessHandleBase> handle,
         storage::FileSystemURL new_url,
         std::vector<scoped_refptr<FileSystemAccessWriteLockManager::WriteLock>>
             write_locks,
         std::unique_ptr<content::SafeMoveHelper> /*safe_move_helper*/,
         base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)>
             callback,
         blink::mojom::FileSystemAccessErrorPtr result) {
        if (handle) {
          DCHECK_CALLED_ON_VALID_SEQUENCE(handle->sequence_checker_);
          if (result->status == blink::mojom::FileSystemAccessStatus::kOk)
            handle->url_ = std::move(new_url);
        }
        // Destroy locks so they are released by the time the callback runs.
        write_locks.clear();
        std::move(callback).Run(std::move(result));
      },
      AsWeakPtr(), dest_url, std::move(locks), std::move(safe_move_helper),
      std::move(callback)));
}

void FileSystemAccessHandleBase::DoRemove(
    const storage::FileSystemURL& url,
    bool recurse,
    WriteLockType lock_type,
    base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  // A locked file cannot be removed. Acquire a write lock and release it
  // after the remove operation completes.
  std::vector<scoped_refptr<WriteLock>> write_locks;
  // TODO(crbug.com/1252614): A directory should only be able to be removed if
  // none of the containing files are locked.
  auto write_lock = manager()->TakeWriteLock(url, lock_type);
  if (!write_lock.has_value()) {
    std::move(callback).Run(file_system_access_error::FromStatus(
        blink::mojom::FileSystemAccessStatus::kNoModificationAllowedError));
    return;
  }
  write_locks.push_back(std::move(write_lock.value()));

  // Bind the `write_locks` to the Remove callback to guarantee the locks are
  // held until the operation completes.
  auto wrapped_callback = base::BindOnce(
      [](std::vector<scoped_refptr<WriteLock>> write_locks,
         base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)>
             callback,
         base::File::Error result) {
        // Destroy locks so they are released by the time the callback runs.
        write_locks.clear();
        std::move(callback).Run(
            file_system_access_error::FromFileError(result));
      },
      std::move(write_locks), std::move(callback));

  manager()->DoFileSystemOperation(FROM_HERE,
                                   &storage::FileSystemOperationRunner::Remove,
                                   std::move(wrapped_callback), url, recurse);
}

// Calculates the parent URL fom current context, propagating any
// storage bucket overrides from the child.
storage::FileSystemURL FileSystemAccessHandleBase::GetParentURL() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const storage::FileSystemURL child = url();
  storage::FileSystemURL parent =
      file_system_context()->CreateCrackedFileSystemURL(
          child.storage_key(), child.mount_type(),
          storage::VirtualPath::DirName(child.virtual_path()));
  if (child.bucket()) {
    parent.SetBucket(child.bucket().value());
  }
  return parent;
}

}  // namespace content
