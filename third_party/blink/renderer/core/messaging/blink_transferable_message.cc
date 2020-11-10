// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/messaging/blink_transferable_message.h"

#include <utility>
#include "mojo/public/cpp/base/big_buffer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "third_party/blink/public/mojom/blob/blob.mojom-blink.h"
#include "third_party/blink/renderer/core/events/message_event.h"
#include "third_party/blink/renderer/core/frame/user_activation.h"
#include "third_party/blink/renderer/core/imagebitmap/image_bitmap.h"
#include "third_party/blink/renderer/platform/blob/blob_data.h"
#include "third_party/blink/renderer/platform/graphics/unaccelerated_static_bitmap_image.h"

#include "third_party/blink/public/web/web_dom_message_event.h"

namespace blink {

// static
BlinkTransferableMessage BlinkTransferableMessage::FromMessageEvent(
    MessageEvent* message_event,
    base::Optional<base::UnguessableToken> cluster_id) {
  BlinkTransferableMessage result;
  SerializedScriptValue* serialized_script_value =
      message_event->DataAsSerializedScriptValue();

  // Message data and cluster ID (optional).
  base::span<const uint8_t> message_wire_data =
      serialized_script_value->GetWireData();
  result.message = SerializedScriptValue::Create(
      reinterpret_cast<const char*>(message_wire_data.data()),
      message_wire_data.size());
  result.locked_agent_cluster_id = cluster_id;

  // Ports
  Vector<MessagePortChannel> ports = message_event->ReleaseChannels();
  result.ports.AppendRange(ports.begin(), ports.end());

  // User activation
  UserActivation* user_activation = message_event->userActivation();
  if (user_activation) {
    result.user_activation = mojom::blink::UserActivationSnapshot::New(
        user_activation->hasBeenActive(), user_activation->isActive());
  }

  // Blobs.
  for (const auto& blob : serialized_script_value->BlobDataHandles()) {
    result.message->BlobDataHandles().Set(
        blob.value->Uuid(),
        BlobDataHandle::Create(blob.value->Uuid(), blob.value->GetType(),
                               blob.value->size(),
                               mojo::PendingRemote<mojom::blink::Blob>(
                                   blob.value->CloneBlobRemote().PassPipe(),
                                   mojom::blink::Blob::Version_)));
  }

  // Stream channels.
  for (auto& stream : serialized_script_value->GetStreams()) {
    result.message->GetStreams().push_back(std::move(stream));
  }
  // Array buffer contents array.
  auto& source_array_buffer_contents_array =
      serialized_script_value->GetArrayBufferContentsArray();
  if (!source_array_buffer_contents_array.IsEmpty()) {
    SerializedScriptValue::ArrayBufferContentsArray array_buffer_contents_array;
    array_buffer_contents_array.ReserveInitialCapacity(
        base::checked_cast<wtf_size_t>(
            source_array_buffer_contents_array.size()));

    for (auto& source_contents : source_array_buffer_contents_array) {
      uint8_t* allocation_start = static_cast<uint8_t*>(source_contents.Data());
      mojo_base::BigBuffer buffer(
          base::make_span(allocation_start, source_contents.DataLength()));
      ArrayBufferContents contents(buffer.size(), 1,
                                   ArrayBufferContents::kNotShared,
                                   ArrayBufferContents::kDontInitialize);
      // Check if we allocated the backing store of the ArrayBufferContents
      // correctly.
      CHECK_EQ(contents.DataLength(), buffer.size());
      memcpy(contents.Data(), buffer.data(), buffer.size());
      array_buffer_contents_array.push_back(std::move(contents));
    }
    result.message->SetArrayBufferContentsArray(
        std::move(array_buffer_contents_array));
  }

  // Image bitmap contents array.
  auto& source_image_bitmap_contents_array =
      serialized_script_value->GetImageBitmapContentsArray();
  if (!source_image_bitmap_contents_array.IsEmpty()) {
    SerializedScriptValue::ImageBitmapContentsArray image_bitmap_contents_array;
    image_bitmap_contents_array.ReserveInitialCapacity(
        base::checked_cast<wtf_size_t>(
            source_image_bitmap_contents_array.size()));

    for (auto& contents : image_bitmap_contents_array) {
      base::Optional<SkBitmap> sk_bitmap = ToSkBitmap(contents);
      if (!sk_bitmap)
        continue;

      const scoped_refptr<StaticBitmapImage> bitmap_contents =
          ToStaticBitmapImage(sk_bitmap.value());
      if (!bitmap_contents)
        continue;
      image_bitmap_contents_array.push_back(bitmap_contents);
    }
    result.message->SetImageBitmapContentsArray(
        std::move(image_bitmap_contents_array));
  }

  // Native file system transfer tokens.
  for (auto& token : serialized_script_value->NativeFileSystemTokens()) {
    uint32_t token_version = token.version();
    mojo::PendingRemote<mojom::blink::NativeFileSystemTransferToken>
        converted_token(token.PassPipe(), token_version);
    result.message->NativeFileSystemTokens().push_back(
        std::move(converted_token));
  }

  return result;
}

// static
BlinkTransferableMessage BlinkTransferableMessage::FromTransferableMessage(
    TransferableMessage message) {
  BlinkTransferableMessage result;
  result.message = SerializedScriptValue::Create(
      reinterpret_cast<const char*>(message.encoded_message.data()),
      message.encoded_message.size());
  for (auto& blob : message.blobs) {
    result.message->BlobDataHandles().Set(
        String::FromUTF8(blob->uuid),
        BlobDataHandle::Create(
            String::FromUTF8(blob->uuid), String::FromUTF8(blob->content_type),
            blob->size,
            mojo::PendingRemote<mojom::blink::Blob>(blob->blob.PassPipe(),
                                                    mojom::Blob::Version_)));
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
  result.locked_agent_cluster_id = message.locked_agent_cluster_id;
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

  if (!message.array_buffer_contents_array.empty()) {
    SerializedScriptValue::ArrayBufferContentsArray array_buffer_contents_array;
    array_buffer_contents_array.ReserveInitialCapacity(
        base::checked_cast<wtf_size_t>(
            message.array_buffer_contents_array.size()));

    for (auto& item : message.array_buffer_contents_array) {
      mojo_base::BigBuffer& big_buffer = item->contents;
      ArrayBufferContents contents(big_buffer.size(), 1,
                                   ArrayBufferContents::kNotShared,
                                   ArrayBufferContents::kDontInitialize);
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

    for (auto& sk_bitmap : message.image_bitmap_contents_array) {
      const scoped_refptr<StaticBitmapImage> bitmap_contents =
          ToStaticBitmapImage(sk_bitmap);
      if (!bitmap_contents)
        continue;
      image_bitmap_contents_array.push_back(bitmap_contents);
    }
    result.message->SetImageBitmapContentsArray(
        std::move(image_bitmap_contents_array));
  }

  // Convert the PendingRemote<NativeFileSystemTransferToken> from the
  // blink::mojom namespace to the blink::mojom::blink namespace.
  for (auto& native_file_system_token : message.native_file_system_tokens) {
    uint32_t token_version = native_file_system_token.version();
    mojo::PendingRemote<mojom::blink::NativeFileSystemTransferToken>
        converted_token(native_file_system_token.PassPipe(), token_version);
    result.message->NativeFileSystemTokens().push_back(
        std::move(converted_token));
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
  sk_sp<SkImage> image = SkImage::MakeFromBitmap(sk_bitmap);
  if (!image)
    return nullptr;

  return UnacceleratedStaticBitmapImage::Create(std::move(image));
}

base::Optional<SkBitmap> ToSkBitmap(
    const scoped_refptr<blink::StaticBitmapImage>& static_bitmap_image) {
  const sk_sp<SkImage> image =
      static_bitmap_image->PaintImageForCurrentFrame().GetSwSkImage();
  SkBitmap result;
  if (image && image->asLegacyBitmap(
                   &result, SkImage::LegacyBitmapMode::kRO_LegacyBitmapMode)) {
    return result;
  }
  return base::nullopt;
}

}  // namespace blink
