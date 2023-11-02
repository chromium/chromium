// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_handle_base.h"

#include <memory>
#include <vector>

#include "base/barrier_callback.h"
#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/file_system_access/features.h"
#include "content/browser/file_system_access/file_system_access_directory_handle_impl.h"
#include "content/browser/file_system_access/file_system_access_error.h"
#include "content/browser/file_system_access/file_system_access_file_handle_impl.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/browser/file_system_access/file_system_access_safe_move_helper.h"
#include "content/browser/file_system_access/file_system_access_transfer_token_impl.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/file_system_access_permission_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "mojo/public/cpp/bindings/message.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/common/file_system/file_system_types.h"
#include "storage/common/file_system/file_system_util.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_error.mojom-forward.h"

namespace content {

using LockHandle = FileSystemAccessLockManager::LockHandle;

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

  if (ShouldTrackUsage(url_)) {
    DCHECK(url_.mount_type() == storage::kFileSystemTypeLocal ||
           url_.mount_type() == storage::kFileSystemTypeExternal)
        << url_.mount_type();

    WebContents* web_contents =
        WebContentsImpl::FromRenderFrameHostID(context_.frame_id);
    if (web_contents) {
      web_contents_ = web_contents->GetWeakPtr();
      static_cast<WebContentsImpl*>(web_contents_.get())
          ->IncrementFileSystemAccessHandleCount();
    }
  }
}

FileSystemAccessHandleBase::~FileSystemAccessHandleBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (ShouldTrackUsage(url_) && web_contents_) {
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
  if (read_status != PermissionStatus::GRANTED) {
    return read_status;
  }

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
  if (current_status != PermissionStatus::ASK || context_.is_worker) {
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
    case Outcome::kGrantedByRestorePrompt:
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
  // Must have write access to the entry being moved. However, write access to
  // the parent directory is not required for renames.
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  if (!FileSystemAccessDirectoryHandleImpl::IsSafePathComponent(
          url().type(), new_entry_name)) {
    std::move(callback).Run(file_system_access_error::FromStatus(
        blink::mojom::FileSystemAccessStatus::kInvalidArgument));
    return;
  }

  auto dest_parent_url = GetParentURL();
  auto dir_handle = std::make_unique<FileSystemAccessDirectoryHandleImpl>(
      manager(), context(), dest_parent_url,
      manager()->GetSharedHandleStateForPath(
          dest_parent_url.path(), context_.storage_key,
          FileSystemAccessPermissionContext::HandleType::kDirectory,
          FileSystemAccessPermissionContext::UserAction::kNone));

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

  if (!FileSystemAccessDirectoryHandleImpl::IsSafePathComponent(
          resolved_destination_directory->url().type(), new_entry_name)) {
    std::move(callback).Run(file_system_access_error::FromStatus(
        blink::mojom::FileSystemAccessStatus::kInvalidArgument));
    return;
  }

  std::unique_ptr<FileSystemAccessDirectoryHandleImpl> dir_handle =
      resolved_destination_directory->CreateDirectoryHandle(context_);

  // Must have write access to the target directory for cross-directory moves.
  if (dir_handle->GetWritePermissionStatus() !=
      blink::mojom::PermissionStatus::GRANTED) {
    std::move(callback).Run(file_system_access_error::FromStatus(
        blink::mojom::FileSystemAccessStatus::kPermissionDenied));
    return;
  }

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

  storage::FileSystemURL dest_url;
  blink::mojom::FileSystemAccessErrorPtr error =
      dir_handle->GetChildURL(new_entry_name, &dest_url);
  if (error != file_system_access_error::Ok()) {
    std::move(callback).Run(std::move(error));
    return;
  }

  // Disallow moves either to or from a sandboxed file system.
  if (url().type() != dest_url.type() &&
      (url().type() == storage::FileSystemType::kFileSystemTypeTemporary ||
       dest_url.type() == storage::FileSystemType::kFileSystemTypeTemporary)) {
    std::move(callback).Run(file_system_access_error::FromStatus(
        blink::mojom::FileSystemAccessStatus::kInvalidModificationError));
    return;
  }

  if (dest_url == url()) {
    // Nothing to do here.
    std::move(callback).Run(file_system_access_error::Ok());
    return;
  }

  // TODO(crbug.com/1250534): Update this to support directory moves.
  auto dest_handle = std::make_unique<FileSystemAccessFileHandleImpl>(
      manager(), context(), dest_url,
      manager()->GetSharedHandleStateForPath(
          dest_url.path(), context_.storage_key,
          FileSystemAccessPermissionContext::HandleType::kFile,
          FileSystemAccessPermissionContext::UserAction::kNone));
  // The site cannot write to the destination entry if write access is
  // explicitly denied.
  auto dest_status = dest_handle->GetWritePermissionStatus();
  if (dest_status == blink::mojom::PermissionStatus::DENIED) {
    std::move(callback).Run(file_system_access_error::FromStatus(
        blink::mojom::FileSystemAccessStatus::kPermissionDenied));
    return;
  }

  // Ideally we would just check the status of `dest_handle` here, which will
  // inherit the permissions of its parent if its permission state is not yet
  // set. However, this permission inheritance logic is not included in the
  // MockFileSystemAccessPermissionGrants our unit tests use. This is safe
  // because, due to permission inheritance, a GRANTED state for the parent
  // implies a GRANTED state for the child, as long as the child is not
  // explicitly DENIED (which is checked above).
  auto has_write_access =
      dest_status == blink::mojom::PermissionStatus::GRANTED ||
      dir_handle->GetWritePermissionStatus() ==
          blink::mojom::PermissionStatus::GRANTED;
  // Require a user gesture if the write access to the destination is not
  // explicitly granted.
  if (!has_write_access && !has_transient_user_activation) {
    DCHECK_NE(dest_status, blink::mojom::PermissionStatus::DENIED);
    // Files in the OPFS always have write access and should never be gated on a
    // user gesture requirement.
    DCHECK_NE(dest_url.type(), storage::kFileSystemTypeTemporary);
    std::move(callback).Run(file_system_access_error::FromStatus(
        blink::mojom::FileSystemAccessStatus::kPermissionDenied));
    return;
  }

  // The file can only be moved if we can acquire exclusive locks to both the
  // source and destination URLs.
  CHECK(dest_url != url());
  auto barrier_callback = base::BarrierCallback<scoped_refptr<LockHandle>>(
      2, base::BindOnce(&FileSystemAccessHandleBase::DidTakeMoveLocks,
                        AsWeakPtr(), dest_url, has_transient_user_activation,
                        has_write_access, std::move(callback)));
  manager()->TakeLock(context(), url(), manager()->GetExclusiveLockType(),
                      barrier_callback);
  manager()->TakeLock(context(), dest_url, manager()->GetExclusiveLockType(),
                      barrier_callback);
}

void FileSystemAccessHandleBase::DidTakeMoveLocks(
    storage::FileSystemURL dest_url,
    bool has_transient_user_activation,
    bool has_write_access,
    base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)> callback,
    std::vector<scoped_refptr<LockHandle>> locks) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  const auto& source_lock = locks[0];
  const auto& dest_lock = locks[1];

  if (!source_lock) {
    std::move(callback).Run(file_system_access_error::FromStatus(
        blink::mojom::FileSystemAccessStatus::kNoModificationAllowedError,
        base::StrCat(
            {"Failed to move ", GetURLDisplayName(url()),
             ". A FileSystemHandle cannot be moved while it is locked."})));
    return;
  }
  if (!dest_lock) {
    std::move(callback).Run(file_system_access_error::FromStatus(
        blink::mojom::FileSystemAccessStatus::kNoModificationAllowedError,
        base::StrCat({"Failed to move ", GetURLDisplayName(url()), " to ",
                      GetURLDisplayName(dest_url),
                      ". A FileSystemHandle cannot be moved to a destination "
                      "which is locked."})));
    return;
  }
  // Only allow overwriting moves if we have write access to the destination or
  // its parent.
  if (has_write_access) {
    DoPerformMoveOperation(dest_url, std::move(locks),
                           has_transient_user_activation, std::move(callback));
    return;
  }

  // TODO(crbug.com/1250534): Use DirectoryExists() for directory moves.
  manager()->DoFileSystemOperation(
      FROM_HERE, &storage::FileSystemOperationRunner::FileExists,
      base::BindOnce(
          &FileSystemAccessHandleBase::ConfirmMoveWillNotOverwriteDestination,
          AsWeakPtr(), has_write_access, dest_url, std::move(locks),
          has_transient_user_activation, std::move(callback)),
      dest_url);
}

void FileSystemAccessHandleBase::ConfirmMoveWillNotOverwriteDestination(
    const bool has_write_access,
    const storage::FileSystemURL& destination_url,
    std::vector<scoped_refptr<LockHandle>> locks,
    bool has_transient_user_activation,
    base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)> callback,
    base::File::Error result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result == base::File::FILE_OK ||
      result == base::File::FILE_ERROR_EXISTS ||
      result == base::File::FILE_ERROR_NOT_EMPTY ||
      result == base::File::FILE_ERROR_NOT_A_FILE ||
      result == base::File::FILE_ERROR_INVALID_OPERATION) {
    std::move(callback).Run(file_system_access_error::FromStatus(
        has_write_access
            ? blink::mojom::FileSystemAccessStatus::kInvalidModificationError
            : blink::mojom::FileSystemAccessStatus::kPermissionDenied));
    return;
  }

  DoPerformMoveOperation(destination_url, std::move(locks),
                         has_transient_user_activation, std::move(callback));
}

void FileSystemAccessHandleBase::DoPerformMoveOperation(
    const storage::FileSystemURL& destination_url,
    std::vector<scoped_refptr<LockHandle>> locks,
    bool has_transient_user_activation,
    base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto file_system_access_safe_move_helper =
      std::make_unique<FileSystemAccessSafeMoveHelper>(
          manager()->AsWeakPtr(), context(), url(), destination_url,
          storage::FileSystemOperation::CopyOrMoveOptionSet(),
          GetContentClient()->browser()->GetQuarantineConnectionCallback(),
          has_transient_user_activation);
  // Allows the unique pointer to be bound to the callback so the helper stays
  // alive until the operation completes.
  FileSystemAccessSafeMoveHelper* raw_helper =
      file_system_access_safe_move_helper.get();
  raw_helper->Start(base::BindOnce(
      &FileSystemAccessHandleBase::DidMove, AsWeakPtr(), destination_url,
      std::move(locks), std::move(file_system_access_safe_move_helper),
      std::move(callback)));
}

void FileSystemAccessHandleBase::DidMove(
    storage::FileSystemURL destination_url,
    std::vector<scoped_refptr<LockHandle>> locks,
    std::unique_ptr<FileSystemAccessSafeMoveHelper> /*move_helper*/,
    base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)> callback,
    blink::mojom::FileSystemAccessErrorPtr result) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (result->status == blink::mojom::FileSystemAccessStatus::kOk) {
    // TODO(crbug.com/1247850): Update permission grants appropriately when
    // moving into/out of the OPFS. Current state:
    // - Moving out of the OPFS: the destination directory is guaranteed to have
    //   write permission. We _should_ update `handle_state_` to point to this
    //   directory, but it technically works as-is.
    // - Moving into the OPFS: we leave a dangling permission grant at the old
    //   path. We should clean this up, but since it's effectively a remove()
    //   this isn't the worst behavior for now.
    if (ShouldTrackUsage(url_) && ShouldTrackUsage(destination_url) &&
        manager()->permission_context()) {
      manager()->permission_context()->NotifyEntryMoved(
          context_.storage_key.origin(), url_.path(), destination_url.path());
    }
    url_ = std::move(destination_url);
  }

  // Destroy locks so they are released by the time the callback runs.
  locks.clear();

  std::move(callback).Run(std::move(result));
}

void FileSystemAccessHandleBase::DoRemove(
    const storage::FileSystemURL& url,
    bool recurse,
    base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  // A locked file cannot be removed. Acquire a lock and release it after the
  // remove operation completes.
  manager()->TakeLock(
      context(), url, manager()->GetExclusiveLockType(),
      base::BindOnce(&FileSystemAccessHandleBase::DidTakeRemoveLock,
                     AsWeakPtr(), url, recurse, std::move(callback)));
}

void FileSystemAccessHandleBase::DidTakeRemoveLock(
    const storage::FileSystemURL& url,
    bool recurse,
    base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)> callback,
    scoped_refptr<LockHandle> lock) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!lock) {
    std::move(callback).Run(file_system_access_error::FromStatus(
        blink::mojom::FileSystemAccessStatus::kNoModificationAllowedError));
    return;
  }

  // Bind the `lock` to the Remove callback to guarantee the lock is held until
  // the operation completes.
  auto wrapped_callback = base::BindOnce(
      [](scoped_refptr<LockHandle> lock,
         base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)>
             callback,
         base::File::Error result) {
        // Destroy lock so it is released by the time the callback runs.
        lock.reset();
        std::move(callback).Run(
            file_system_access_error::FromFileError(result));
      },
      std::move(lock), std::move(callback));

  manager()->DoFileSystemOperation(FROM_HERE,
                                   &storage::FileSystemOperationRunner::Remove,
                                   std::move(wrapped_callback), url, recurse);
}

void FileSystemAccessHandleBase::DoGetCloudIdentifiers(
    FileSystemAccessPermissionContext::HandleType handle_type,
    ContentBrowserClient::GetCloudIdentifiersCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!base::FeatureList::IsEnabled(
          blink::features::kFileSystemAccessGetCloudIdentifiers)) {
    mojo::ReportBadMessage(
        "feature 'FileSystemAccessGetCloudIdentifiers' not enabled");
    std::move(callback).Run(
        file_system_access_error::FromStatus(
            blink::mojom::FileSystemAccessStatus::kSecurityError),
        {});
    return;
  }

  if (GetReadPermissionStatus() != PermissionStatus::GRANTED) {
    std::move(callback).Run(
        file_system_access_error::FromStatus(
            blink::mojom::FileSystemAccessStatus::kPermissionDenied),
        {});
    return;
  }

  GetContentClient()->browser()->GetCloudIdentifiers(url_, handle_type,
                                                     std::move(callback));
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
