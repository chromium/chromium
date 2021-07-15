// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/encoded_audio_chunk.h"

#include <utility>

#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_audio_chunk_init.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

EncodedAudioChunk* EncodedAudioChunk::Create(
    const EncodedAudioChunkInit* init) {
  DOMArrayPiece piece(init->data());
  auto buffer =
      piece.ByteLength()
          ? media::DecoderBuffer::CopyFrom(
                reinterpret_cast<uint8_t*>(piece.Data()), piece.ByteLength())
          : base::MakeRefCounted<media::DecoderBuffer>(0);

  // Clamp within bounds of our internal TimeDelta-based duration. See
  // media/base/timestamp_constants.h
  auto timestamp = base::TimeDelta::FromMicroseconds(init->timestamp());
  if (timestamp == media::kNoTimestamp)
    timestamp = base::TimeDelta::FiniteMin();
  else if (timestamp == media::kInfiniteDuration)
    timestamp = base::TimeDelta::FiniteMax();
  buffer->set_timestamp(timestamp);

  buffer->set_duration(media::kNoTimestamp);
  buffer->set_is_key_frame(init->type() == "key");
  return MakeGarbageCollected<EncodedAudioChunk>(std::move(buffer));
}

EncodedAudioChunk::EncodedAudioChunk(scoped_refptr<media::DecoderBuffer> buffer)
    : buffer_(std::move(buffer)) {}

String EncodedAudioChunk::type() const {
  return buffer_->is_key_frame() ? "key" : "delta";
}

int64_t EncodedAudioChunk::timestamp() const {
  return buffer_->timestamp().InMicroseconds();
}

uint64_t EncodedAudioChunk::byteLength() const {
  return buffer_->data_size();
}

void EncodedAudioChunk::copyTo(const V8BufferSource* destination,
                               ExceptionState& exception_state) {
  // Validate destination buffer.
  DOMArrayPiece dest_wrapper(destination);
  if (dest_wrapper.IsDetached()) {
    exception_state.ThrowTypeError("destination is detached.");
    return;
  }
  if (dest_wrapper.ByteLength() < buffer_->data_size()) {
    exception_state.ThrowTypeError("destination is not large enough.");
    return;
  }

  // Copy data.
  memcpy(dest_wrapper.Bytes(), buffer_->data(), buffer_->data_size());
}

}  // namespace blink
