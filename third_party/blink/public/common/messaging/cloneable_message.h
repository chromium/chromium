// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_CLONEABLE_MESSAGE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_CLONEABLE_MESSAGE_H_

#include <vector>

#include "base/containers/span.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/unguessable_token.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/mojom/blob/serialized_blob.mojom.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_transfer_token.mojom.h"
#include "url/origin.h"

namespace blink {

// This struct represents messages as they are posted over a broadcast channel.
// This type can be serialized as a blink::mojom::CloneableMessage struct.
struct BLINK_COMMON_EXPORT CloneableMessage {
  CloneableMessage();
  CloneableMessage(CloneableMessage&&);
  CloneableMessage& operator=(CloneableMessage&&);
  ~CloneableMessage();

  // Returns a shallow clone of this message. |encoded_message| in the clone
  // will point to the same memory as |encoded_message| in this.
  CloneableMessage ShallowClone() const;

  // To reduce copies when serializing |encoded_message| does not have to point
  // to |owned_encoded_message|. The serialization code completely ignores the
  // |owned_encoded_message| and just serializes whatever |encoded_message|
  // points to. When deserializing |owned_encoded_message| is set to the data
  // and |encoded_message| is set to point to |owned_encoded_message|.
  base::span<const uint8_t> encoded_message;
  std::vector<uint8_t> owned_encoded_message;

  // Copies data into owned_encoded_message if it's not already there.
  void EnsureDataIsOwned();

  // Blob handles for any blobs being sent in this message.
  std::vector<mojom::SerializedBlobPtr> blobs;

  // Populated by message senders to prevent remote origins from accessing
  // |native_file_system_tokens|. Tokens from a remote origin must fail by
  // dispatching a 'messageerror' event instead of a 'message' event. If
  // somehow a compromised process bypasses this origin check, the
  // NativeFileSystemManager in the browser process performs another origin
  // check that will prevent the use of remote origin NativeFileSystemHandles.
  //
  // The message sender may leave |sender_origin| null. When |sender_origin| is
  // null, the message receiver must handle the message like it is from an
  // unknown remote origin by dispatching the 'messageerror' event when
  // |native_file_system_tokens| is not an empty array.
  base::Optional<url::Origin> sender_origin;

  // Stack trace captured by sender.
  // For more details see v8_inspector::V8StackTraceId.
  uint64_t stack_trace_id = 0;
  int64_t stack_trace_debugger_id_first = 0;
  int64_t stack_trace_debugger_id_second = 0;
  bool stack_trace_should_pause = false;

  // If not null, this message is locked to the given agent cluster ID.
  // See
  // https://html.spec.whatwg.org/multipage/webappapis.html#integration-with-the-javascript-agent-cluster-formalism
  base::Optional<base::UnguessableToken> locked_agent_cluster_id;

  // Tokens required to clone FileSystemFileHandles and/or
  // FileSystemDirectoryHandles.
  std::vector<mojo::PendingRemote<mojom::NativeFileSystemTransferToken>>
      native_file_system_tokens;

 private:
  DISALLOW_COPY_AND_ASSIGN(CloneableMessage);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_CLONEABLE_MESSAGE_H_
