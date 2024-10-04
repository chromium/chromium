// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"

#include <utility>
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/blob/blob.mojom-blink.h"
#include "third_party/blink/public/platform/cross_variant_mojo_util.h"
#include "third_party/blink/renderer/core/frame/user_activation.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/graphics/accelerated_static_bitmap_image.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"
#include "third_party/skia/include/core/SkImage.h"

namespace blink {

// static
BlinkTransferableMessage BlinkTransferableMessage::FromTransferableMessage(
    TransferableMessage message) {
  BlinkTransferableMessage result;
  result.message = SerializedScriptValue::Create(message.encoded_message);
  for (auto& blob : message.blobs) {
    result.message->BlobDataHandles().Set(
        String::FromUTF8(blob->uuid),
        BlobDataHandle::Create(String::FromUTF8(blob->uuid),
                               String::FromUTF8(blob->content_type), blob->size,
                               ToCrossVariantMojoType(std::move(blob->blob))));
  }
  if (message.sender_origin) {
    result.sender_origin =
        blink::SecurityOrigin::CreateFromUrlOrigin(*message.sender_origin);
  }
  result.sender_stack_trace_id = v8_inspector::V8StackTraceId(
      static_cast<uintptr_t>(message.stack_trace_id),
      std::make_pair(message.stack_trace_debugger_id_first,
                     message.stack_trace_debugger_id_second),
      message.stack_trace_should_pause);
  result.sender_agent_cluster_id = message.sender_agent_cluster_id;
  result.locked_to_sender_agent_cluster =
      message.locked_to_sender_agent_cluster;
  result.ports.AppendRange(message.ports.begin(), message.ports.end());
  for (auto& channel : message.stream_channels) {
    result.message->GetStreams().push_back(
        SerializedScriptValue::Stream(channel.ReleaseHandle()));
  }
  if (message.user_activation) {
    result.user_activation = mojom::blink::UserActivationSnapshot::New(
        message.user_activation->has_been_active,
        message.user_activation->was_active);
  }
  result.delegated_capability = message.delegated_capability;

  result.parent_task_id = message.parent_task_id;

  if (!message.array_buffer_contents_array.empty()) {
    SerializedScriptValue::ArrayBufferContentsArray array_buffer_contents_array;
    array_buffer_contents_array.ReserveInitialCapacity(
        base::checked_cast<wtf_size_t>(
            message.array_buffer_contents_array.size()));

    for (auto& item : message.array_buffer_contents_array) {
      mojo_base::BigBuffer& big_buffer = item->contents;
      std::optional<size_t> max_byte_length;
      if (item->is_resizable_by_user_javascript) {
        max_byte_length = base::checked_cast<size_t>(item->max_byte_length);
      }
      ArrayBufferContents contents(
          big_buffer.size(), max_byte_length, 1,
          ArrayBufferContents::kNotShared, ArrayBufferContents::kDontInitialize,
          ArrayBufferContents::AllocationFailureBehavior::kCrash);
      // Check if we allocated the backing store of the ArrayBufferContents
      // correctly.
      CHECK_EQ(contents.DataLength(), big_buffer.size());
      memcpy(contents.Data(), big_buffer.data(), big_buffer.size());
      array_buffer_contents_array.push_back(std::move(contents));
    }
    result.message->SetArrayBufferContentsArray(
        std::move(array_buffer_contents_array));
  }

  if (!message.image_bitmap_contents_array.empty()) {
    SerializedScriptValue::ImageBitmapContentsArray image_bitmap_contents_array;
    image_bitmap_contents_array.ReserveInitialCapacity(
        base::checked_cast<wtf_size_t>(
            message.image_bitmap_contents_array.size()));

    for (auto& image : message.image_bitmap_contents_array) {
      if (image->is_bitmap()) {
        const scoped_refptr<StaticBitmapImage> bitmap_contents =
            ToStaticBitmapImage(image->get_bitmap());
        if (!bitmap_contents) {
          continue;
        }
        image_bitmap_contents_array.push_back(bitmap_contents);
      } else if (image->is_accelerated_image()) {
        const scoped_refptr<StaticBitmapImage> accelerated_image =
            WrapAcceleratedBitmapImage(
                std::move(image->get_accelerated_image()));
        if (!accelerated_image) {
          continue;
        }
        image_bitmap_contents_array.push_back(accelerated_image);
      }
    }
    result.message->SetImageBitmapContentsArray(
        std::move(image_bitmap_contents_array));
  }

  // Convert the PendingRemote<FileSystemAccessTransferToken> from the
  // blink::mojom namespace to the blink::mojom::blink namespace.
  for (auto& token : message.file_system_access_tokens) {
    result.message->FileSystemAccessTokens().push_back(
        ToCrossVariantMojoType(std::move(token)));
  }
  return result;
}

BlinkTransferableMessage::BlinkTransferableMessage() = default;
BlinkTransferableMessage::~BlinkTransferableMessage() = default;

BlinkTransferableMessage::BlinkTransferableMessage(BlinkTransferableMessage&&) =
    default;
BlinkTransferableMessage& BlinkTransferableMessage::operator=(
    BlinkTransferableMessage&&) = default;

scoped_refptr<StaticBitmapImage> ToStaticBitmapImage(
    const SkBitmap& sk_bitmap) {
  sk_sp<SkImage> image = SkImages::RasterFromBitmap(sk_bitmap);
  if (!image)
    return nullptr;

  return UnacceleratedStaticBitmapImage::Create(std::move(image));
}

scoped_refptr<StaticBitmapImage> WrapAcceleratedBitmapImage(
    AcceleratedImageInfo image) {
  return AcceleratedStaticBitmapImage::CreateFromExternalSharedImage(
      image.shared_image, image.image_info, image.is_origin_top_left,
      image.supports_display_compositing, image.is_overlay_candidate,
      std::move(image.release_callback));
}
}  // namespace blink
