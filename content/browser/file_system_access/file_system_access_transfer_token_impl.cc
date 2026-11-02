// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/file_system_access/file_system_access_transfer_token_impl.h"

#include <utility>

#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/cross_origin_storage/cross_origin_storage_file_handle_impl.h"
#include "content/browser/cross_origin_storage/cross_origin_storage_registry.h"
#include "content/browser/file_system_access/file_system_access_directory_handle_impl.h"
#include "content/browser/file_system_access/file_system_access_file_handle_impl.h"
#include "content/browser/file_system_access/fixed_file_system_access_permission_grant.h"
#include "storage/common/file_system/file_system_types.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_directory_handle.mojom.h"

namespace content {

using HandleType = FileSystemAccessPermissionContext::HandleType;
using SharedHandleState = FileSystemAccessManagerImpl::SharedHandleState;

namespace {

// Non-sandboxed transfer tokens redeemed into a third-party context (such as
// via `postMessage` or `document.requestStorageAccess({indexedDB: true})`) must
// not inherit first-party permission grants.
SharedHandleState GetEffectiveHandleStateForContext(
    const storage::FileSystemURL& url,
    const FileSystemAccessManagerImpl::BindingContext& binding_context,
    const SharedHandleState& handle_state) {
  if (url.type() != storage::kFileSystemTypeTemporary &&
      binding_context.storage_key.IsThirdPartyContext()) {
    auto denied_grant =
        base::MakeRefCounted<FixedFileSystemAccessPermissionGrant>(
            blink::mojom::PermissionStatus::DENIED, PathInfo(url.path()));
    return SharedHandleState(denied_grant, denied_grant);
  }
  return handle_state;
}

}  // namespace

FileSystemAccessTransferTokenImpl::CrossOriginStorageData::
    CrossOriginStorageData(
        base::WeakPtr<CrossOriginStorageRegistry> registry,
        scoped_refptr<ChromeBlobStorageContext> blob_storage_context,
        CrossOriginStorageHash hash,
        CrossOriginStorageAccessSpec access_spec,
        bool is_writable,
        bool may_read)
    : registry(std::move(registry)),
      blob_storage_context(std::move(blob_storage_context)),
      hash(std::move(hash)),
      access_spec(std::move(access_spec)),
      is_writable(is_writable),
      may_read(may_read) {}

FileSystemAccessTransferTokenImpl::CrossOriginStorageData::
    CrossOriginStorageData(const CrossOriginStorageData&) = default;
FileSystemAccessTransferTokenImpl::CrossOriginStorageData&
FileSystemAccessTransferTokenImpl::CrossOriginStorageData::operator=(
    const CrossOriginStorageData&) = default;
FileSystemAccessTransferTokenImpl::CrossOriginStorageData::
    CrossOriginStorageData(CrossOriginStorageData&&) = default;
FileSystemAccessTransferTokenImpl::CrossOriginStorageData&
FileSystemAccessTransferTokenImpl::CrossOriginStorageData::operator=(
    CrossOriginStorageData&&) = default;
FileSystemAccessTransferTokenImpl::CrossOriginStorageData::
    ~CrossOriginStorageData() = default;

FileSystemAccessTransferTokenImpl::FileSystemAccessTransferTokenImpl(
    const storage::FileSystemURL& url,
    const url::Origin& origin,
    const std::string& display_name,
    const FileSystemAccessManagerImpl::SharedHandleState& handle_state,
    HandleType handle_type,
    FileSystemAccessManagerImpl* manager,
    mojo::PendingReceiver<blink::mojom::FileSystemAccessTransferToken> receiver)
    : token_(base::UnguessableToken::Create()),
      handle_type_(handle_type),
      manager_(manager),
      url_(url),
      origin_(origin),
      display_name_(display_name),
      handle_state_(handle_state) {
  CHECK(manager_, base::NotFatalUntil::M159);
  CHECK(url.origin().opaque() || url.origin() == origin,
        base::NotFatalUntil::M159);

  receivers_.set_disconnect_handler(
      base::BindRepeating(&FileSystemAccessTransferTokenImpl::OnMojoDisconnect,
                          base::Unretained(this)));

  receivers_.Add(this, std::move(receiver));
}

FileSystemAccessTransferTokenImpl::FileSystemAccessTransferTokenImpl(
    const url::Origin& origin,
    CrossOriginStorageData cross_origin_storage_data,
    FileSystemAccessManagerImpl* manager,
    mojo::PendingReceiver<blink::mojom::FileSystemAccessTransferToken> receiver)
    : token_(base::UnguessableToken::Create()),
      handle_type_(HandleType::kFile),
      manager_(manager),
      origin_(origin),
      cross_origin_storage_data_(std::move(cross_origin_storage_data)) {
  CHECK(manager_, base::NotFatalUntil::M159);

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
  CHECK_EQ(handle_type_, HandleType::kFile, base::NotFatalUntil::M159);
  CHECK(handle_state_.has_value());
  return std::make_unique<FileSystemAccessFileHandleImpl>(
      manager_, binding_context, url_, display_name_,
      GetEffectiveHandleStateForContext(url_, binding_context, *handle_state_));
}

std::unique_ptr<CrossOriginStorageFileHandleImpl>
FileSystemAccessTransferTokenImpl::CreateCrossOriginStorageFileHandle() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(cross_origin_storage_data_.has_value());
  return std::make_unique<CrossOriginStorageFileHandleImpl>(
      cross_origin_storage_data_->registry,
      cross_origin_storage_data_->blob_storage_context, manager_->AsWeakPtr(),
      cross_origin_storage_data_->hash, origin_,
      cross_origin_storage_data_->access_spec,
      cross_origin_storage_data_->is_writable,
      cross_origin_storage_data_->may_read);
}

std::unique_ptr<FileSystemAccessDirectoryHandleImpl>
FileSystemAccessTransferTokenImpl::CreateDirectoryHandle(
    const FileSystemAccessManagerImpl::BindingContext& binding_context) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK_EQ(handle_type_, HandleType::kDirectory, base::NotFatalUntil::M159);
  CHECK(handle_state_.has_value());
  return std::make_unique<FileSystemAccessDirectoryHandleImpl>(
      manager_, binding_context, url_,
      GetEffectiveHandleStateForContext(url_, binding_context, *handle_state_));
}

FileSystemAccessPermissionGrant*
FileSystemAccessTransferTokenImpl::GetReadGrant() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return handle_state_ ? handle_state_->read_grant.get() : nullptr;
}

FileSystemAccessPermissionGrant*
FileSystemAccessTransferTokenImpl::GetWriteGrant() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return handle_state_ ? handle_state_->write_grant.get() : nullptr;
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
