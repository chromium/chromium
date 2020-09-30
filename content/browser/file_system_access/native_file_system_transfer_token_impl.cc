// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/native_file_system_transfer_token_impl.h"

#include "content/browser/file_system_access/native_file_system_directory_handle_impl.h"
#include "content/browser/file_system_access/native_file_system_file_handle_impl.h"
#include "third_party/blink/public/mojom/file_system_access/native_file_system_directory_handle.mojom.h"

namespace content {

using HandleType = NativeFileSystemPermissionContext::HandleType;
using SharedHandleState = NativeFileSystemManagerImpl::SharedHandleState;

NativeFileSystemTransferTokenImpl::NativeFileSystemTransferTokenImpl(
    const storage::FileSystemURL& url,
    const url::Origin& origin,
    const NativeFileSystemManagerImpl::SharedHandleState& handle_state,
    HandleType handle_type,
    NativeFileSystemManagerImpl* manager,
    mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken> receiver)
    : token_(base::UnguessableToken::Create()),
      handle_type_(handle_type),
      manager_(manager),
      url_(url),
      origin_(origin),
      handle_state_(handle_state) {
  DCHECK(manager_);
  DCHECK(url.origin().opaque() || url.origin() == origin);

  receivers_.set_disconnect_handler(
      base::BindRepeating(&NativeFileSystemTransferTokenImpl::OnMojoDisconnect,
                          base::Unretained(this)));

  receivers_.Add(this, std::move(receiver));
}

NativeFileSystemTransferTokenImpl::~NativeFileSystemTransferTokenImpl() =
    default;

std::unique_ptr<NativeFileSystemFileHandleImpl>
NativeFileSystemTransferTokenImpl::CreateFileHandle(
    const NativeFileSystemManagerImpl::BindingContext& binding_context) const {
  DCHECK_EQ(handle_type_, HandleType::kFile);
  return std::make_unique<NativeFileSystemFileHandleImpl>(
      manager_, binding_context, url_, handle_state_);
}

std::unique_ptr<NativeFileSystemDirectoryHandleImpl>
NativeFileSystemTransferTokenImpl::CreateDirectoryHandle(
    const NativeFileSystemManagerImpl::BindingContext& binding_context) const {
  DCHECK_EQ(handle_type_, HandleType::kDirectory);
  return std::make_unique<NativeFileSystemDirectoryHandleImpl>(
      manager_, binding_context, url_, handle_state_);
}

NativeFileSystemPermissionGrant*
NativeFileSystemTransferTokenImpl::GetReadGrant() const {
  return handle_state_.read_grant.get();
}

NativeFileSystemPermissionGrant*
NativeFileSystemTransferTokenImpl::GetWriteGrant() const {
  return handle_state_.write_grant.get();
}

void NativeFileSystemTransferTokenImpl::GetInternalID(
    GetInternalIDCallback callback) {
  std::move(callback).Run(token_);
}

void NativeFileSystemTransferTokenImpl::OnMojoDisconnect() {
  if (receivers_.empty()) {
    manager_->RemoveToken(token_);
  }
}

void NativeFileSystemTransferTokenImpl::Clone(
    mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken>
        clone_receiver) {
  receivers_.Add(this, std::move(clone_receiver));
}

}  // namespace content
