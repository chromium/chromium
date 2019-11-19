// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/messaging/cloneable_message.h"

#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/messaging/cloneable_message.mojom.h"

namespace blink {

CloneableMessage::CloneableMessage() = default;
CloneableMessage::CloneableMessage(CloneableMessage&&) = default;
CloneableMessage& CloneableMessage::operator=(CloneableMessage&&) = default;
CloneableMessage::~CloneableMessage() = default;

CloneableMessage CloneableMessage::ShallowClone() const {
  CloneableMessage clone;
  clone.encoded_message = encoded_message;

  // Both |blobs| and |native_file_system_tokens| contain mojo pending remotes.
  // ShallowClone() follows these steps to clone each pending remote:
  //
  // (1) Temporarily bind the source pending remote in this CloneableMessage's
  // |blobs| or |native_file_system_tokens|.  This requires a const_cast because
  // it temporarily modifies this CloneableMessage's |blobs| or
  // |native_file_system_tokens|.
  //
  // (2) Use the bound remote to call Clone(), which creates a new remote for
  // the new clone.
  //
  // (3) Unbind the source remote to restore this CloneableMessage's |blobs| or
  // |native_file_system_tokens| back to the original pending remote from (1).
  for (const auto& blob : blobs) {
    mojom::SerializedBlobPtr& source_serialized_blob =
        const_cast<mojom::SerializedBlobPtr&>(blob);

    mojo::Remote<mojom::Blob> source_blob(
        std::move(source_serialized_blob->blob));

    mojo::PendingRemote<mojom::Blob> cloned_blob;
    source_blob->Clone(cloned_blob.InitWithNewPipeAndPassReceiver());

    clone.blobs.push_back(mojom::SerializedBlob::New(
        source_serialized_blob->uuid, source_serialized_blob->content_type,
        source_serialized_blob->size, std::move(cloned_blob)));

    source_serialized_blob->blob = source_blob.Unbind();
  }

  // Clone the |native_file_system_tokens| pending remotes using the steps
  // described by the comment above.
  std::vector<mojo::PendingRemote<mojom::NativeFileSystemTransferToken>>&
      source_tokens = const_cast<std::vector<
          mojo::PendingRemote<mojom::NativeFileSystemTransferToken>>&>(
          native_file_system_tokens);

  for (size_t i = 0; i < source_tokens.size(); ++i) {
    mojo::Remote<mojom::NativeFileSystemTransferToken> source_token(
        std::move(source_tokens[i]));

    mojo::PendingRemote<mojom::NativeFileSystemTransferToken> cloned_token;
    source_token->Clone(cloned_token.InitWithNewPipeAndPassReceiver());

    clone.native_file_system_tokens.push_back(std::move(cloned_token));
    source_tokens[i] = source_token.Unbind();
  }
  return clone;
}

void CloneableMessage::EnsureDataIsOwned() {
  if (encoded_message.data() == owned_encoded_message.data())
    return;
  owned_encoded_message.assign(encoded_message.begin(), encoded_message.end());
  encoded_message = owned_encoded_message;
}

}  // namespace blink
