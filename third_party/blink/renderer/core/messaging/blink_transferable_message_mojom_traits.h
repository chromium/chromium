// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_MESSAGING_BLINK_TRANSFERABLE_MESSAGE_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_MESSAGING_BLINK_TRANSFERABLE_MESSAGE_MOJOM_TRAITS_H_

#include "mojo/public/cpp/bindings/array_traits_wtf_vector.h"
#include "skia/public/mojom/bitmap_skbitmap_mojom_traits.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/mojom/messaging/transferable_message.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/messaging/blink_cloneable_message_mojom_traits.h"
#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"

namespace mojo {

template <>
struct CORE_EXPORT
    StructTraits<blink::mojom::blink::TransferableMessage::DataView,
                 blink::BlinkTransferableMessage> {
  static blink::BlinkCloneableMessage& message(
      blink::BlinkTransferableMessage& input) {
    return input;
  }

  static Vector<mojo::ScopedMessagePipeHandle> ports(
      blink::BlinkTransferableMessage& input) {
    Vector<mojo::ScopedMessagePipeHandle> result;
    result.ReserveInitialCapacity(input.ports.size());
    for (const auto& port : input.ports)
      result.push_back(port.ReleaseHandle());
    return result;
  }

  static Vector<mojo::ScopedMessagePipeHandle> stream_channels(
      blink::BlinkTransferableMessage& input) {
    Vector<mojo::ScopedMessagePipeHandle> result;
    auto& stream_channels = input.message->GetStreamChannels();
    result.ReserveInitialCapacity(stream_channels.size());
    for (const auto& port : stream_channels)
      result.push_back(port.ReleaseHandle());
    return result;
  }

  static const blink::SerializedScriptValue::ArrayBufferContentsArray&
  array_buffer_contents_array(const blink::BlinkCloneableMessage& input) {
    return input.message->GetArrayBufferContentsArray();
  }

  static Vector<SkBitmap> image_bitmap_contents_array(
      const blink::BlinkCloneableMessage& input);

  static const blink::mojom::blink::UserActivationSnapshotPtr& user_activation(
      const blink::BlinkTransferableMessage& input) {
    return input.user_activation;
  }

  static bool transfer_user_activation(
      const blink::BlinkTransferableMessage& input) {
    return input.transfer_user_activation;
  }

  static bool allow_autoplay(const blink::BlinkTransferableMessage& input) {
    return input.allow_autoplay;
  }

  static bool Read(blink::mojom::blink::TransferableMessage::DataView,
                   blink::BlinkTransferableMessage* out);
};

template <>
class CORE_EXPORT
    StructTraits<blink::mojom::blink::SerializedArrayBufferContents::DataView,
                 blink::ArrayBufferContents> {
 public:
  static mojo_base::BigBuffer contents(
      const blink::ArrayBufferContents& array_buffer_contents) {
    uint8_t* allocation_start =
        static_cast<uint8_t*>(array_buffer_contents.Data());
    return mojo_base::BigBuffer(
        base::make_span(allocation_start, array_buffer_contents.DataLength()));
  }
  static bool Read(blink::mojom::blink::SerializedArrayBufferContents::DataView,
                   blink::ArrayBufferContents* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_MESSAGING_BLINK_TRANSFERABLE_MESSAGE_MOJOM_TRAITS_H_
