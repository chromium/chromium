// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/native_file_system_handle_base.h"

#include "base/task/post_task.h"
#include "content/browser/file_system_access/native_file_system_error.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/back_forward_cache.h"
#include "content/public/browser/browser_task_traits.h"
#include "storage/common/file_system/file_system_types.h"

namespace content {

NativeFileSystemHandleBase::NativeFileSystemHandleBase(
    NativeFileSystemManagerImpl* manager,
    const BindingContext& context,
    const storage::FileSystemURL& url,
    const SharedHandleState& handle_state)
    : manager_(manager),
      context_(context),
      url_(url),
      handle_state_(handle_state) {
  DCHECK(manager_);
  DCHECK_EQ(url_.mount_type() == storage::kFileSystemTypeIsolated,
            handle_state_.file_system.is_valid())
      << url_.mount_type();

  // We support sandboxed file system and native file systems on all platforms.
  DCHECK(url_.type() == storage::kFileSystemTypeNativeLocal ||
         url_.type() == storage::kFileSystemTypeTemporary ||
         url_.mount_type() == storage::kFileSystemTypeExternal ||
         url_.type() == storage::kFileSystemTypeTest)
      << url_.type();

  if (ShouldTrackUsage()) {
    DCHECK(url_.mount_type() == storage::kFileSystemTypeIsolated ||
           url_.mount_type() == storage::kFileSystemTypeExternal)
        << url_.mount_type();
    if (url_.mount_type() == storage::kFileSystemTypeIsolated)
      DCHECK_EQ(url_.type(), storage::kFileSystemTypeNativeLocal);

    Observe(WebContentsImpl::FromRenderFrameHostID(context_.frame_id));

    // Disable back-forward cache as native file system's usage of
    // RenderFrameHost::IsCurrent at the moment is not compatible with bfcache.
    BackForwardCache::DisableForRenderFrameHost(context_.frame_id,
                                                "NativeFileSystem");
    if (web_contents())
      web_contents()->IncrementNativeFileSystemHandleCount();
  }
}

NativeFileSystemHandleBase::~NativeFileSystemHandleBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (ShouldTrackUsage() && web_contents()) {
    web_contents()->DecrementNativeFileSystemHandleCount();
  }
}

NativeFileSystemHandleBase::PermissionStatus
NativeFileSystemHandleBase::GetReadPermissionStatus() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return handle_state_.read_grant->GetStatus();
}

NativeFileSystemHandleBase::PermissionStatus
NativeFileSystemHandleBase::GetWritePermissionStatus() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // It is not currently possible to have write only handles, so first check the
  // read permission status. See also:
  // http://wicg.github.io/native-file-system/#api-filesystemhandle-querypermission
  PermissionStatus read_status = GetReadPermissionStatus();
  if (read_status != PermissionStatus::GRANTED)
    return read_status;

  return handle_state_.write_grant->GetStatus();
}

void NativeFileSystemHandleBase::DoGetPermissionStatus(
    bool writable,
    base::OnceCallback<void(PermissionStatus)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(writable ? GetWritePermissionStatus()
                                   : GetReadPermissionStatus());
}

void NativeFileSystemHandleBase::DoRequestPermission(
    bool writable,
    base::OnceCallback<void(blink::mojom::NativeFileSystemErrorPtr,
                            PermissionStatus)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  PermissionStatus current_status =
      writable ? GetWritePermissionStatus() : GetReadPermissionStatus();
  // If we already have a valid permission status, just return that. Also just
  // return the current permission status if this is called from a worker, as we
  // don't support prompting for increased permissions from workers.
  //
  // Currently the worker check here is redundant because there is no way for
  // workers to get native file system handles. While workers will never be able
  // to call chooseEntries(), they will be able to receive existing handles from
  // windows via postMessage() and IndexedDB.
  if (current_status != PermissionStatus::ASK || context_.is_worker()) {
    std::move(callback).Run(native_file_system_error::Ok(), current_status);
    return;
  }
  if (!writable) {
    handle_state_.read_grant->RequestPermission(
        context().frame_id,
        NativeFileSystemPermissionGrant::UserActivationState::kRequired,
        base::BindOnce(&NativeFileSystemHandleBase::DidRequestPermission,
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
        NativeFileSystemPermissionGrant::UserActivationState::kRequired,
        base::DoNothing());
  }

  handle_state_.write_grant->RequestPermission(
      context().frame_id,
      NativeFileSystemPermissionGrant::UserActivationState::kRequired,
      base::BindOnce(&NativeFileSystemHandleBase::DidRequestPermission,
                     AsWeakPtr(), writable, std::move(callback)));
}

void NativeFileSystemHandleBase::DidRequestPermission(
    bool writable,
    base::OnceCallback<void(blink::mojom::NativeFileSystemErrorPtr,
                            PermissionStatus)> callback,
    NativeFileSystemPermissionGrant::PermissionRequestOutcome outcome) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  using Outcome = NativeFileSystemPermissionGrant::PermissionRequestOutcome;
  switch (outcome) {
    case Outcome::kInvalidFrame:
    case Outcome::kThirdPartyContext:
      std::move(callback).Run(
          native_file_system_error::FromStatus(
              blink::mojom::NativeFileSystemStatus::kSecurityError,
              "Not allowed to request permissions in this context."),
          writable ? GetWritePermissionStatus() : GetReadPermissionStatus());
      return;
    case Outcome::kNoUserActivation:
      std::move(callback).Run(
          native_file_system_error::FromStatus(
              blink::mojom::NativeFileSystemStatus::kSecurityError,
              "User activation is required to request permissions."),
          writable ? GetWritePermissionStatus() : GetReadPermissionStatus());
      return;
    case Outcome::kBlockedByContentSetting:
    case Outcome::kUserGranted:
    case Outcome::kUserDenied:
    case Outcome::kUserDismissed:
    case Outcome::kRequestAborted:
    case Outcome::kGrantedByContentSetting:
      std::move(callback).Run(
          native_file_system_error::Ok(),
          writable ? GetWritePermissionStatus() : GetReadPermissionStatus());
      return;
  }
  NOTREACHED();
}

}  // namespace content
