// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_TRANSFER_TOKEN_IMPL_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_TRANSFER_TOKEN_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/thread_annotations.h"
#include "content/browser/file_system_access/file_system_access_manager_impl.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/isolated_context.h"
#include "third_party/blink/public/mojom/file_system_access/file_system_access_transfer_token.mojom.h"

namespace content {

// This is the browser side implementation of the FileSystemAccessTransferToken
// mojom interface. These tokens are tied to a particular origin
// and use the permission grants in their `handle_state_` member when creating
// new handles. They are used for postMessage and IndexedDB, serialization as
// well as a couple of other APIs.
//
// Instances of this class should always be used from the sequence they were
// created on.
class CONTENT_EXPORT FileSystemAccessTransferTokenImpl
    : public blink::mojom::FileSystemAccessTransferToken {
 public:
  FileSystemAccessTransferTokenImpl(
      const storage::FileSystemURL& url,
      const url::Origin& origin,
      const FileSystemAccessManagerImpl::SharedHandleState& handle_state,
      FileSystemAccessPermissionContext::HandleType handle_type,
      FileSystemAccessManagerImpl* manager,
      mojo::PendingReceiver<blink::mojom::FileSystemAccessTransferToken>
          receiver);
  FileSystemAccessTransferTokenImpl(const FileSystemAccessTransferTokenImpl&) =
      delete;
  FileSystemAccessTransferTokenImpl& operator=(
      const FileSystemAccessTransferTokenImpl&) = delete;
  ~FileSystemAccessTransferTokenImpl() override;

  const base::UnguessableToken& token() const { return token_; }
  FileSystemAccessPermissionContext::HandleType type() const {
    return handle_type_;
  }
  const storage::FileSystemURL& url() const { return url_; }
  const url::Origin& origin() const { return origin_; }

  // Returns permission grants associated with this token. These can
  // return nullptr if this token does not have associated permission grants.
  FileSystemAccessPermissionGrant* GetReadGrant() const;
  FileSystemAccessPermissionGrant* GetWriteGrant() const;

  std::unique_ptr<FileSystemAccessFileHandleImpl> CreateFileHandle(
      const FileSystemAccessManagerImpl::BindingContext& binding_context) const;
  std::unique_ptr<FileSystemAccessDirectoryHandleImpl> CreateDirectoryHandle(
      const FileSystemAccessManagerImpl::BindingContext& binding_context) const;

  // blink::mojom::FileSystemAccessTransferToken:
  void GetInternalID(GetInternalIDCallback callback) override;
  void Clone(mojo::PendingReceiver<blink::mojom::FileSystemAccessTransferToken>
                 clone_receiver) override;

 private:
  void OnMojoDisconnect();

  SEQUENCE_CHECKER(sequence_checker_);

  // This token may contain multiple receivers, which includes a receiver for
  // the originally constructed instance and then additional receivers for
  // each clone. `manager_` must not remove this token until `receivers_` is
  // empty.
  const base::UnguessableToken token_;
  const FileSystemAccessPermissionContext::HandleType handle_type_;
  // Raw pointer since FileSystemAccessManagerImpl owns `this`.
  const raw_ptr<FileSystemAccessManagerImpl> manager_ = nullptr;
  const storage::FileSystemURL url_;
  const url::Origin origin_;
  const FileSystemAccessManagerImpl::SharedHandleState handle_state_;
  mojo::ReceiverSet<blink::mojom::FileSystemAccessTransferToken> receivers_
      GUARDED_BY_CONTEXT(sequence_checker_);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_FILE_SYSTEM_ACCESS_TRANSFER_TOKEN_IMPL_H_
