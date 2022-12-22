// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/messaging/blink_transferable_message_mojom_traits.h"

#include "mojo/public/cpp/base/big_buffer_mojom_traits.h"
#include "skia/ext/skia_utils_base.h"
#include "third_party/blink/public/mojom/messaging/transferable_message.mojom-blink.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace mojo {

namespace {

absl::optional<SkBitmap> ToSkBitmapN32(
    const scoped_refptr<blink::StaticBitmapImage>& static_bitmap_image) {
  const sk_sp<SkImage> image =
      static_bitmap_image->PaintImageForCurrentFrame().GetSwSkImage();
  if (!image)
    return absl::nullopt;

  SkBitmap sk_bitmap;
  if (!image->asLegacyBitmap(&sk_bitmap,
                             SkImage::LegacyBitmapMode::kRO_LegacyBitmapMode)) {
    return absl::nullopt;
  }

  SkBitmap sk_bitmap_n32;
  if (!skia::SkBitmapToN32OpaqueOrPremul(sk_bitmap, &sk_bitmap_n32)) {
    return absl::nullopt;
  }

  return sk_bitmap_n32;
}

}  // namespace

Vector<SkBitmap>
StructTraits<blink::mojom::blink::TransferableMessage::DataView,
             blink::BlinkTransferableMessage>::
    image_bitmap_contents_array(const blink::BlinkCloneableMessage& input) {
  Vector<SkBitmap> out;
  out.ReserveInitialCapacity(
      input.message->GetImageBitmapContentsArray().size());
  for (auto& bitmap_contents : input.message->GetImageBitmapContentsArray()) {
    // TransferableMessage::image_bitmap_contents_array is an array of
    // skia.mojom.BitmapN32, so SkBitmap should be in N32 format.
    auto bitmap_n32 = ToSkBitmapN32(bitmap_contents);
    if (!bitmap_n32) {
      return Vector<SkBitmap>();
    }
    out.push_back(std::move(bitmap_n32.value()));
  }
  return out;
}

bool StructTraits<blink::mojom::blink::TransferableMessage::DataView,
                  blink::BlinkTransferableMessage>::
    Read(blink::mojom::blink::TransferableMessage::DataView data,
         blink::BlinkTransferableMessage* out) {
  Vector<blink::MessagePortDescriptor> ports;
  Vector<blink::MessagePortDescriptor> stream_channels;
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
  for (auto& channel : stream_channels) {
    out->message->GetStreams().push_back(
        blink::SerializedScriptValue::Stream(std::move(channel)));
  }

  out->delegated_capability = data.delegated_capability();

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

  absl::optional<size_t> max_data_size;
  if (data.is_resizable_by_user_javascript()) {
    max_data_size = base::checked_cast<size_t>(data.max_byte_length());
  }
  blink::ArrayBufferContents array_buffer_contents(
      contents_data.size(), max_data_size, 1,
      blink::ArrayBufferContents::kNotShared,
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
