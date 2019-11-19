// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_TRANSFERABLE_MESSAGE_MOJOM_TRAITS_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_TRANSFERABLE_MESSAGE_MOJOM_TRAITS_H_

#include "skia/public/mojom/bitmap_skbitmap_mojom_traits.h"
#include "third_party/blink/public/common/messaging/cloneable_message_mojom_traits.h"
#include "third_party/blink/public/common/messaging/transferable_message.h"
#include "third_party/blink/public/mojom/messaging/transferable_message.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace mojo {

template <>
struct BLINK_COMMON_EXPORT
    StructTraits<blink::mojom::TransferableMessage::DataView,
                 blink::TransferableMessage> {
  static blink::CloneableMessage& message(blink::TransferableMessage& input) {
    return input;
  }

  static std::vector<mojo::ScopedMessagePipeHandle> ports(
      blink::TransferableMessage& input) {
    return blink::MessagePortChannel::ReleaseHandles(input.ports);
  }

  static std::vector<mojo::ScopedMessagePipeHandle> stream_channels(
      blink::TransferableMessage& input) {
    return blink::MessagePortChannel::ReleaseHandles(input.stream_channels);
  }

  static std::vector<blink::mojom::SerializedArrayBufferContentsPtr>
  array_buffer_contents_array(blink::TransferableMessage& input) {
    return std::move(input.array_buffer_contents_array);
  }

  static const std::vector<SkBitmap>& image_bitmap_contents_array(
      blink::TransferableMessage& input) {
    return input.image_bitmap_contents_array;
  }

  static const blink::mojom::UserActivationSnapshotPtr& user_activation(
      blink::TransferableMessage& input) {
    return input.user_activation;
  }

  static bool transfer_user_activation(blink::TransferableMessage& input) {
    return input.transfer_user_activation;
  }

  static bool allow_autoplay(blink::TransferableMessage& input) {
    return input.allow_autoplay;
  }

  static bool Read(blink::mojom::TransferableMessage::DataView data,
                   blink::TransferableMessage* out);
};

}  // namespace mojo

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_TRANSFERABLE_MESSAGE_MOJOM_TRAITS_H_
