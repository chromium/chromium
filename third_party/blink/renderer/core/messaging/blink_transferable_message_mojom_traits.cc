// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/messaging/blink_transferable_message_mojom_traits.h"

#include "mojo/public/cpp/base/big_buffer_mojom_traits.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace mojo {

Vector<SkBitmap>
StructTraits<blink::mojom::blink::TransferableMessage::DataView,
             blink::BlinkTransferableMessage>::
    image_bitmap_contents_array(const blink::BlinkCloneableMessage& input) {
  Vector<SkBitmap> out;
  out.ReserveInitialCapacity(
      input.message->GetImageBitmapContentsArray().size());
  for (auto& bitmap_contents : input.message->GetImageBitmapContentsArray()) {
    base::Optional<SkBitmap> bitmap = blink::ToSkBitmap(bitmap_contents);
    if (!bitmap) {
      return Vector<SkBitmap>();
    }
    out.push_back(std::move(bitmap.value()));
  }
  return out;
}

bool StructTraits<blink::mojom::blink::TransferableMessage::DataView,
                  blink::BlinkTransferableMessage>::
    Read(blink::mojom::blink::TransferableMessage::DataView data,
         blink::BlinkTransferableMessage* out) {
  Vector<mojo::ScopedMessagePipeHandle> ports;
  Vector<mojo::ScopedMessagePipeHandle> stream_channels;
  blink::SerializedScriptValue::ArrayBufferContentsArray
      array_buffer_contents_array;
  Vector<SkBitmap> sk_bitmaps;
  if (!data.ReadMessage(static_cast<blink::BlinkCloneableMessage*>(out)) ||
      !data.ReadArrayBufferContentsArray(&array_buffer_contents_array) ||
      !data.ReadImageBitmapContentsArray(&sk_bitmaps) ||
      !data.ReadPorts(&ports) || !data.ReadStreamChannels(&stream_channels) ||
      !data.ReadUserActivation(&out->user_activation)) {
    return false;
  }

  out->ports.ReserveInitialCapacity(ports.size());
  out->ports.AppendRange(std::make_move_iterator(ports.begin()),
                         std::make_move_iterator(ports.end()));
  out->message->GetStreamChannels().AppendRange(
      std::make_move_iterator(stream_channels.begin()),
      std::make_move_iterator(stream_channels.end()));
  out->transfer_user_activation = data.transfer_user_activation();
  out->allow_autoplay = data.allow_autoplay();

  out->message->SetArrayBufferContentsArray(
      std::move(array_buffer_contents_array));
  array_buffer_contents_array.clear();

  // Bitmaps are serialized in mojo as SkBitmaps to leverage existing
  // serialization logic, but SerializedScriptValue uses StaticBitmapImage, so
  // the SkBitmaps need to be converted to StaticBitmapImages.
  blink::SerializedScriptValue::ImageBitmapContentsArray
      image_bitmap_contents_array;
  for (auto& sk_bitmap : sk_bitmaps) {
    const scoped_refptr<blink::StaticBitmapImage> bitmap_contents =
        blink::ToStaticBitmapImage(sk_bitmap);
    if (!bitmap_contents) {
      return false;
    }
    image_bitmap_contents_array.push_back(bitmap_contents);
  }
  out->message->SetImageBitmapContentsArray(image_bitmap_contents_array);
  return true;
}

bool StructTraits<blink::mojom::blink::SerializedArrayBufferContents::DataView,
                  blink::ArrayBufferContents>::
    Read(blink::mojom::blink::SerializedArrayBufferContents::DataView data,
         blink::ArrayBufferContents* out) {
  mojo_base::BigBufferView contents_view;
  if (!data.ReadContents(&contents_view))
    return false;
  auto contents_data = contents_view.data();

  blink::ArrayBufferContents array_buffer_contents(
      contents_data.size(), 1, blink::ArrayBufferContents::kNotShared,
      blink::ArrayBufferContents::kDontInitialize);
  if (contents_data.size() != array_buffer_contents.DataLength()) {
    return false;
  }
  memcpy(array_buffer_contents.Data(), contents_data.data(),
         contents_data.size());
  *out = std::move(array_buffer_contents);
  return true;
}

}  // namespace mojo
