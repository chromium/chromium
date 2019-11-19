// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_TRANSFER_TOKEN_IMPL_H_
#define CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_TRANSFER_TOKEN_IMPL_H_

#include "content/browser/native_file_system/native_file_system_manager_impl.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/isolated_context.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_transfer_token.mojom.h"

namespace content {

// This is the browser side implementation of the NativeFileSystemTransferToken
// mojom interface.
//
// Instances of this class are immutable, but since this implements a mojo
// interface all its methods are called on the same sequence anyway.
class NativeFileSystemTransferTokenImpl
    : public blink::mojom::NativeFileSystemTransferToken {
 public:
  using SharedHandleState = NativeFileSystemManagerImpl::SharedHandleState;

  enum class HandleType { kFile, kDirectory };

  NativeFileSystemTransferTokenImpl(
      const storage::FileSystemURL& url,
      const SharedHandleState& handle_state,
      HandleType type,
      NativeFileSystemManagerImpl* manager,
      mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken>
          receiver);
  ~NativeFileSystemTransferTokenImpl() override;

  const base::UnguessableToken& token() const { return token_; }
  const SharedHandleState& shared_handle_state() const { return handle_state_; }
  const storage::FileSystemURL& url() const { return url_; }
  HandleType type() const { return type_; }

  // blink::mojom::NativeFileSystemTransferToken:
  void GetInternalID(GetInternalIDCallback callback) override;
  void Clone(mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken>
                 clone_receiver) override;

 private:
  void OnMojoDisconnect();

  const base::UnguessableToken token_;
  const storage::FileSystemURL url_;
  const SharedHandleState handle_state_;
  const HandleType type_;
  // Raw pointer since NativeFileSystemManagerImpl owns |this|.
  NativeFileSystemManagerImpl* const manager_;

  // This token may contain multiple receivers, which includes a receiver for
  // the originally constructed instance and then additional receivers for
  // each clone. |manager_| must not remove this token until |receivers_| is
  // empty.
  mojo::ReceiverSet<blink::mojom::NativeFileSystemTransferToken> receivers_;

  DISALLOW_COPY_AND_ASSIGN(NativeFileSystemTransferTokenImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NATIVE_FILE_SYSTEM_NATIVE_FILE_SYSTEM_TRANSFER_TOKEN_IMPL_H_
