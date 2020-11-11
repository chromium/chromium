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
  EncodedAudioMetadata metadata;
  metadata.timestamp = base::TimeDelta::FromMicroseconds(init->timestamp());
  metadata.key_frame = (init->type() == "key");
  DOMArrayPiece piece(init->data());

  // A full copy of the data happens here.
  auto* buffer = piece.IsNull()
                     ? nullptr
                     : DOMArrayBuffer::Create(piece.Data(), piece.ByteLength());
  return MakeGarbageCollected<EncodedAudioChunk>(metadata, buffer);
}

EncodedAudioChunk::EncodedAudioChunk(EncodedAudioMetadata metadata,
                                     DOMArrayBuffer* buffer)
    : metadata_(metadata), buffer_(buffer) {}

String EncodedAudioChunk::type() const {
  return metadata_.key_frame ? "key" : "delta";
}

uint64_t EncodedAudioChunk::timestamp() const {
  return metadata_.timestamp.InMicroseconds();
}

DOMArrayBuffer* EncodedAudioChunk::data() const {
  return buffer_;
}

}  // namespace blink
