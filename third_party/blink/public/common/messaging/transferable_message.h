// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_TRANSFERABLE_MESSAGE_H_
#define THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_TRANSFERABLE_MESSAGE_H_

#include <vector>

#include "base/containers/span.h"
#include "base/macros.h"
#include "third_party/blink/public/common/common_export.h"
#include "third_party/blink/public/common/messaging/cloneable_message.h"
#include "third_party/blink/public/common/messaging/message_port_channel.h"
#include "third_party/blink/public/mojom/array_buffer/array_buffer_contents.mojom.h"
#include "third_party/blink/public/mojom/messaging/user_activation_snapshot.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace blink {

// This struct represents messages as they are posted over a message port. This
// type can be serialized as a blink::mojom::TransferableMessage struct.
struct BLINK_COMMON_EXPORT TransferableMessage : public CloneableMessage {
  TransferableMessage();
  TransferableMessage(TransferableMessage&&);
  TransferableMessage& operator=(TransferableMessage&&);
  ~TransferableMessage();

  // Any ports being transferred as part of this message.
  std::vector<MessagePortChannel> ports;
  // Channels used by transferred WHATWG streams (eg. ReadableStream).
  std::vector<MessagePortChannel> stream_channels;
  // The contents of any ArrayBuffers being transferred as part of this message.
  std::vector<mojom::SerializedArrayBufferContentsPtr>
      array_buffer_contents_array;
  // The contents of any ImageBitmaps being transferred as part of this message.
  std::vector<SkBitmap> image_bitmap_contents_array;

  // The state of user activation.
  mojom::UserActivationSnapshotPtr user_activation;

  // Whether the state of user activation should be transferred to the
  // destination frame.
  bool transfer_user_activation = false;

  // Whether the destination frame is allowed to autoplay.
  //
  // TODO(mustaq): Ideally the |transfer_user_activation| field above should be
  // replaced by bits specific to "safe-to-delegate" capabilities, like the
  // autoplay bit below.  See crbug.com/985914.
  bool allow_autoplay = false;

 private:
  DISALLOW_COPY_AND_ASSIGN(TransferableMessage);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_PUBLIC_COMMON_MESSAGING_TRANSFERABLE_MESSAGE_H_
