// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/encoded_video_chunk.h"

#include <utility>

#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_chunk_init.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

EncodedVideoChunk* EncodedVideoChunk::Create(EncodedVideoChunkInit* init) {
  auto data_wrapper = AsSpan<const uint8_t>(init->data());
  auto buffer = data_wrapper.empty()
                    ? base::MakeRefCounted<media::DecoderBuffer>(0)
                    : media::DecoderBuffer::CopyFrom(data_wrapper.data(),
                                                     data_wrapper.size());

  // Clamp within bounds of our internal TimeDelta-based duration. See
  // media/base/timestamp_constants.h
  auto timestamp = base::Microseconds(init->timestamp());
  if (timestamp == media::kNoTimestamp)
    timestamp = base::TimeDelta::FiniteMin();
  else if (timestamp == media::kInfiniteDuration)
    timestamp = base::TimeDelta::FiniteMax();
  buffer->set_timestamp(timestamp);

  // media::kNoTimestamp corresponds to base::TimeDelta::Min(), and internally
  // denotes the absence of duration. We use base::TimeDelta::FiniteMax() --
  // which is one less than base::TimeDelta::Max() -- because
  // base::TimeDelta::Max() is reserved for media::kInfiniteDuration, and is
  // handled differently.
  buffer->set_duration(
      init->hasDuration()
          ? base::Microseconds(std::min(
                uint64_t{base::TimeDelta::FiniteMax().InMicroseconds()},
                init->duration()))
          : media::kNoTimestamp);

  buffer->set_is_key_frame(init->type() == "key");
  return MakeGarbageCollected<EncodedVideoChunk>(std::move(buffer));
}

EncodedVideoChunk::EncodedVideoChunk(scoped_refptr<media::DecoderBuffer> buffer)
    : buffer_(std::move(buffer)) {}

String EncodedVideoChunk::type() const {
  return buffer_->is_key_frame() ? "key" : "delta";
}

int64_t EncodedVideoChunk::timestamp() const {
  return buffer_->timestamp().InMicroseconds();
}

absl::optional<uint64_t> EncodedVideoChunk::duration() const {
  if (buffer_->duration() == media::kNoTimestamp)
    return absl::nullopt;
  return buffer_->duration().InMicroseconds();
}

uint64_t EncodedVideoChunk::byteLength() const {
  return buffer_->data_size();
}

void EncodedVideoChunk::copyTo(const AllowSharedBufferSource* destination,
                               ExceptionState& exception_state) {
  // Validate destination buffer.
  auto dest_wrapper = AsSpan<uint8_t>(destination);
  if (dest_wrapper.size() < buffer_->data_size()) {
    exception_state.ThrowTypeError("destination is not large enough.");
    return;
  }

  if (buffer_->data_size() == 0) {
    // Calling memcpy with nullptr is UB, even if count is zero.
    return;
  }

  // Copy data.
  memcpy(dest_wrapper.data(), buffer_->data(), buffer_->data_size());
}

}  // namespace blink
