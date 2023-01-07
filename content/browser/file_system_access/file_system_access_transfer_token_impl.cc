// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_transfer_token_impl.h"

#include "content/browser/file_system_access/file_system_access_directory_handle_impl.h"
#include "content/browser/file_system_access/file_system_access_file_handle_impl.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_directory_handle.mojom.h"

namespace content {

using HandleType = FileSystemAccessPermissionContext::HandleType;
using SharedHandleState = FileSystemAccessManagerImpl::SharedHandleState;

FileSystemAccessTransferTokenImpl::FileSystemAccessTransferTokenImpl(
    const storage::FileSystemURL& url,
    const url::Origin& origin,
    const FileSystemAccessManagerImpl::SharedHandleState& handle_state,
    HandleType handle_type,
    FileSystemAccessManagerImpl* manager,
    mojo::PendingReceiver<blink::mojom::FileSystemAccessTransferToken> receiver)
    : token_(base::UnguessableToken::Create()),
      handle_type_(handle_type),
      manager_(manager),
      url_(url),
      origin_(origin),
      handle_state_(handle_state) {
  DCHECK(manager_);
  DCHECK(url.origin().opaque() || url.origin() == origin);

  receivers_.set_disconnect_handler(
      base::BindRepeating(&FileSystemAccessTransferTokenImpl::OnMojoDisconnect,
                          base::Unretained(this)));

  receivers_.Add(this, std::move(receiver));
}

FileSystemAccessTransferTokenImpl::~FileSystemAccessTransferTokenImpl() =
    default;

std::unique_ptr<FileSystemAccessFileHandleImpl>
FileSystemAccessTransferTokenImpl::CreateFileHandle(
    const FileSystemAccessManagerImpl::BindingContext& binding_context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(handle_type_, HandleType::kFile);
  return std::make_unique<FileSystemAccessFileHandleImpl>(
      manager_, binding_context, url_, handle_state_);
}

std::unique_ptr<FileSystemAccessDirectoryHandleImpl>
FileSystemAccessTransferTokenImpl::CreateDirectoryHandle(
    const FileSystemAccessManagerImpl::BindingContext& binding_context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(handle_type_, HandleType::kDirectory);
  return std::make_unique<FileSystemAccessDirectoryHandleImpl>(
      manager_, binding_context, url_, handle_state_);
}

FileSystemAccessPermissionGrant*
FileSystemAccessTransferTokenImpl::GetReadGrant() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return handle_state_.read_grant.get();
}

FileSystemAccessPermissionGrant*
FileSystemAccessTransferTokenImpl::GetWriteGrant() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return handle_state_.write_grant.get();
}

void FileSystemAccessTransferTokenImpl::GetInternalID(
    GetInternalIDCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::move(callback).Run(token_);
}

void FileSystemAccessTransferTokenImpl::OnMojoDisconnect() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (receivers_.empty()) {
    manager_->RemoveToken(token_);
  }
}

void FileSystemAccessTransferTokenImpl::Clone(
    mojo::PendingReceiver<blink::mojom::FileSystemAccessTransferToken>
        clone_receiver) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  receivers_.Add(this, std::move(clone_receiver));
}

}  // namespace content
