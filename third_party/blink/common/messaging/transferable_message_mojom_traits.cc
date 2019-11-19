// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/messaging/transferable_message_mojom_traits.h"

#include "base/containers/span.h"
#include "third_party/blink/public/common/messaging/cloneable_message_mojom_traits.h"

namespace mojo {

bool StructTraits<blink::mojom::TransferableMessage::DataView,
                  blink::TransferableMessage>::
    Read(blink::mojom::TransferableMessage::DataView data,
         blink::TransferableMessage* out) {
  std::vector<mojo::ScopedMessagePipeHandle> ports;
  std::vector<mojo::ScopedMessagePipeHandle> stream_channels;
  if (!data.ReadMessage(static_cast<blink::CloneableMessage*>(out)) ||
      !data.ReadArrayBufferContentsArray(&out->array_buffer_contents_array) ||
      !data.ReadImageBitmapContentsArray(&out->image_bitmap_contents_array) ||
      !data.ReadPorts(&ports) || !data.ReadStreamChannels(&stream_channels) ||
      !data.ReadUserActivation(&out->user_activation)) {
    return false;
  }

  out->ports = blink::MessagePortChannel::CreateFromHandles(std::move(ports));
  out->stream_channels =
      blink::MessagePortChannel::CreateFromHandles(std::move(stream_channels));
  out->transfer_user_activation = data.transfer_user_activation();
  out->allow_autoplay = data.allow_autoplay();
  return true;
}

}  // namespace mojo
