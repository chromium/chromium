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
  EncodedVideoMetadata metadata;
  metadata.timestamp = base::TimeDelta::FromMicroseconds(init->timestamp());
  metadata.key_frame = (init->type() == "key");
  if (init->hasDurationNonNull()) {
    metadata.duration =
        base::TimeDelta::FromMicroseconds(init->durationNonNull());
  }
  DOMArrayPiece piece(init->data());

  // A full copy of the data happens here.
  auto* buffer = piece.IsNull() ? nullptr
                                : DOMArrayBuffer::Create(
                                      piece.Data(), piece.ByteLengthAsSizeT());
  return MakeGarbageCollected<EncodedVideoChunk>(metadata, buffer);
}

EncodedVideoChunk::EncodedVideoChunk(EncodedVideoMetadata metadata,
                                     DOMArrayBuffer* buffer)
    : metadata_(metadata), buffer_(buffer) {}

String EncodedVideoChunk::type() const {
  return metadata_.key_frame ? "key" : "delta";
}

uint64_t EncodedVideoChunk::timestamp() const {
  return metadata_.timestamp.InMicroseconds();
}

base::Optional<uint64_t> EncodedVideoChunk::duration() const {
  if (!metadata_.duration)
    return base::nullopt;
  return metadata_.duration->InMicroseconds();
}

DOMArrayBuffer* EncodedVideoChunk::data() const {
  return buffer_;
}

}  // namespace blink
