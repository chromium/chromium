// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_handle_base.h"

#include "base/task/post_task.h"
#include "content/browser/file_system_access/file_system_access_error.h"
#include "content/browser/renderer_host/back_forward_cache_disable.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_task_traits.h"
#include "storage/common/file_system/file_system_types.h"

namespace content {

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

    Observe(WebContentsImpl::FromRenderFrameHostID(context_.frame_id));

    // Disable back-forward cache as File System Access's usage of
    // RenderFrameHost::IsActive at the moment is not compatible with bfcache.
    BackForwardCache::DisableForRenderFrameHost(
        context_.frame_id,
        BackForwardCacheDisable::DisabledReason(
            BackForwardCacheDisable::DisabledReasonId::kFileSystemAccess));
    if (web_contents())
      web_contents()->IncrementFileSystemAccessHandleCount();
  }
}

FileSystemAccessHandleBase::~FileSystemAccessHandleBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (ShouldTrackUsage() && web_contents()) {
    web_contents()->DecrementFileSystemAccessHandleCount();
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

void FileSystemAccessHandleBase::DoRemove(
    const storage::FileSystemURL& url,
    bool recurse,
    base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  DoFileSystemOperation(
      FROM_HERE, &storage::FileSystemOperationRunner::Remove,
      base::BindOnce(
          [](base::OnceCallback<void(blink::mojom::FileSystemAccessErrorPtr)>
                 callback,
             base::File::Error result) {
            std::move(callback).Run(
                file_system_access_error::FromFileError(result));
          },
          std::move(callback)),
      url, recurse);
}

WebContentsImpl* FileSystemAccessHandleBase::web_contents() const {
  return static_cast<WebContentsImpl*>(WebContentsObserver::web_contents());
}

}  // namespace content
