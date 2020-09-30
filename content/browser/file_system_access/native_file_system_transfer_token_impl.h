// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FILE_SYSTEM_ACCESS_NATIVE_FILE_SYSTEM_TRANSFER_TOKEN_IMPL_H_
#define CONTENT_BROWSER_FILE_SYSTEM_ACCESS_NATIVE_FILE_SYSTEM_TRANSFER_TOKEN_IMPL_H_

#include "content/browser/file_system_access/native_file_system_manager_impl.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/isolated_context.h"
#include "third_party/blink/public/mojom/file_system_access/native_file_system_transfer_token.mojom.h"

namespace content {

// This is the browser side implementation of the NativeFileSystemTransferToken
// mojom interface. These tokens are tied to a particular origin
// and use the permission grants in their `handle_state_` member when creating
// new handles. They are used for postMessage and IndexedDB, serialization as
// well as a couple of other APIs.
//
// Instances of this class should always be used from the sequence they were
// created on.
class CONTENT_EXPORT NativeFileSystemTransferTokenImpl
    : public blink::mojom::NativeFileSystemTransferToken {
 public:
  NativeFileSystemTransferTokenImpl(
      const storage::FileSystemURL& url,
      const url::Origin& origin,
      const NativeFileSystemManagerImpl::SharedHandleState& handle_state,
      NativeFileSystemPermissionContext::HandleType handle_type,
      NativeFileSystemManagerImpl* manager,
      mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken>
          receiver);
  ~NativeFileSystemTransferTokenImpl() override;

  const base::UnguessableToken& token() const { return token_; }
  NativeFileSystemPermissionContext::HandleType type() const {
    return handle_type_;
  }
  const storage::FileSystemURL& url() const { return url_; }
  const url::Origin& origin() const { return origin_; }

  // Returns permission grants associated with this token. These can
  // return nullptr if this token does not have associated permission grants.
  NativeFileSystemPermissionGrant* GetReadGrant() const;
  NativeFileSystemPermissionGrant* GetWriteGrant() const;

  std::unique_ptr<NativeFileSystemFileHandleImpl> CreateFileHandle(
      const NativeFileSystemManagerImpl::BindingContext& binding_context) const;
  std::unique_ptr<NativeFileSystemDirectoryHandleImpl> CreateDirectoryHandle(
      const NativeFileSystemManagerImpl::BindingContext& binding_context) const;

  // blink::mojom::NativeFileSystemTransferToken:
  void GetInternalID(GetInternalIDCallback callback) override;
  void Clone(mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken>
                 clone_receiver) override;

 private:
  void OnMojoDisconnect();

  // This token may contain multiple receivers, which includes a receiver for
  // the originally constructed instance and then additional receivers for
  // each clone. `manager_` must not remove this token until `receivers_` is
  // empty.
  const base::UnguessableToken token_;
  const NativeFileSystemPermissionContext::HandleType handle_type_;
  // Raw pointer since NativeFileSystemManagerImpl owns `this`.
  NativeFileSystemManagerImpl* const manager_;
  const storage::FileSystemURL url_;
  const url::Origin origin_;
  const NativeFileSystemManagerImpl::SharedHandleState handle_state_;
  mojo::ReceiverSet<blink::mojom::NativeFileSystemTransferToken> receivers_;

  DISALLOW_COPY_AND_ASSIGN(NativeFileSystemTransferTokenImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FILE_SYSTEM_ACCESS_NATIVE_FILE_SYSTEM_TRANSFER_TOKEN_IMPL_H_
