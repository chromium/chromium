// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/encoded_video_chunk.h"

#include <utility>

#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_chunk_init.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_buffer.h"
#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

namespace blink {

EncodedVideoChunk* EncodedVideoChunk::Create(EncodedVideoChunkInit* init) {
  auto timestamp = base::TimeDelta::FromMicroseconds(init->timestamp());
  bool key_frame = (init->type() == "key");
  DOMArrayPiece piece(init->data());

  // A full copy of the data happens here.
  auto* buffer = piece.IsNull()
                     ? nullptr
                     : DOMArrayBuffer::Create(piece.Data(), piece.ByteLength());
  auto* result =
      MakeGarbageCollected<EncodedVideoChunk>(timestamp, key_frame, buffer);
  if (init->hasDuration())
    result->duration_ = base::TimeDelta::FromMicroseconds(init->duration());
  return result;
}

EncodedVideoChunk::EncodedVideoChunk(base::TimeDelta timestamp,
                                     bool key_frame,
                                     DOMArrayBuffer* buffer)
    : timestamp_(timestamp), key_frame_(key_frame), buffer_(buffer) {}

String EncodedVideoChunk::type() const {
  return key_frame_ ? "key" : "delta";
}

uint64_t EncodedVideoChunk::timestamp() const {
  return timestamp_.InMicroseconds();
}

base::Optional<uint64_t> EncodedVideoChunk::duration() const {
  if (!duration_.has_value())
    return base::nullopt;
  return duration_->InMicroseconds();
}

DOMArrayBuffer* EncodedVideoChunk::data() const {
  return buffer_;
}

}  // namespace blink
