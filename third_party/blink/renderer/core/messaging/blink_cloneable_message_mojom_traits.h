// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_MESSAGING_BLINK_CLONEABLE_MESSAGE_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_MESSAGING_BLINK_CLONEABLE_MESSAGE_MOJOM_TRAITS_H_

#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/base/unguessable_token_mojom_traits.h"
#include "mojo/public/cpp/bindings/array_traits_wtf_vector.h"
#include "mojo/public/cpp/bindings/string_traits_wtf.h"
#include "third_party/blink/public/mojom/messaging/cloneable_message.mojom-blink.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_transfer_token.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/messaging/blink_cloneable_message.h"
#include "third_party/blink/renderer/platform/blob/serialized_blob_mojom_traits.h"
#include "third_party/blink/renderer/platform/mojo/security_origin_mojom_traits.h"

namespace mojo {

template <>
struct CORE_EXPORT StructTraits<blink::mojom::blink::CloneableMessage::DataView,
                                blink::BlinkCloneableMessage> {
  static mojo_base::BigBuffer encoded_message(
      blink::BlinkCloneableMessage& input) {
    return mojo_base::BigBuffer(input.message->GetWireData());
  }

  static Vector<scoped_refptr<blink::BlobDataHandle>> blobs(
      blink::BlinkCloneableMessage& input);

  static const scoped_refptr<const blink::SecurityOrigin>& sender_origin(
      blink::BlinkCloneableMessage& input) {
    return input.sender_origin;
  }

  static uint64_t stack_trace_id(const blink::BlinkCloneableMessage& input) {
    return static_cast<uint64_t>(input.sender_stack_trace_id.id);
  }

  static int64_t stack_trace_debugger_id_first(
      const blink::BlinkCloneableMessage& input) {
    return input.sender_stack_trace_id.debugger_id.first;
  }

  static int64_t stack_trace_debugger_id_second(
      const blink::BlinkCloneableMessage& input) {
    return input.sender_stack_trace_id.debugger_id.second;
  }

  static bool stack_trace_should_pause(
      const blink::BlinkCloneableMessage& input) {
    return input.sender_stack_trace_id.should_pause;
  }

  static const base::Optional<base::UnguessableToken>& locked_agent_cluster_id(
      const blink::BlinkCloneableMessage& input) {
    return input.locked_agent_cluster_id;
  }

  static Vector<
      mojo::PendingRemote<blink::mojom::blink::NativeFileSystemTransferToken>>
  native_file_system_tokens(blink::BlinkCloneableMessage& input) {
    return std::move(input.message->NativeFileSystemTokens());
  }

  static bool Read(blink::mojom::blink::CloneableMessage::DataView,
                   blink::BlinkCloneableMessage* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_MESSAGING_BLINK_CLONEABLE_MESSAGE_MOJOM_TRAITS_H_
