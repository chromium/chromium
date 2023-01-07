// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/messaging/transferable_message_mojom_traits.h"

#include "base/containers/span.h"
#include "third_party/blink/public/common/messaging/cloneable_message_mojom_traits.h"
#include "third_party/blink/public/common/messaging/message_port_descriptor.h"

namespace mojo {

bool StructTraits<blink::mojom::TransferableMessage::DataView,
                  blink::TransferableMessage>::
    Read(blink::mojom::TransferableMessage::DataView data,
         blink::TransferableMessage* out) {
  std::vector<blink::MessagePortDescriptor> ports;
  std::vector<blink::MessagePortDescriptor> stream_channels;
  if (!data.ReadMessage(static_cast<blink::CloneableMessage*>(out)) ||
      !data.ReadArrayBufferContentsArray(&out->array_buffer_contents_array) ||
      !data.ReadImageBitmapContentsArray(&out->image_bitmap_contents_array) ||
      !data.ReadPorts(&ports) || !data.ReadStreamChannels(&stream_channels) ||
      !data.ReadUserActivation(&out->user_activation) ||
      !data.ReadParentTaskId(&out->parent_task_id)) {
    return false;
  }

  out->ports = blink::MessagePortChannel::CreateFromHandles(std::move(ports));
  out->stream_channels =
      blink::MessagePortChannel::CreateFromHandles(std::move(stream_channels));
  out->delegated_capability = data.delegated_capability();
  return true;
}

}  // namespace mojo
