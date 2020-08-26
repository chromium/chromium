// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_ENCODED_AUDIO_CHUNK_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_ENCODED_AUDIO_CHUNK_H_

#include "third_party/blink/renderer/core/typed_arrays/dom_array_piece.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_audio_metadata.h"
#include "third_party/blink/renderer/platform/bindings/script_wrappable.h"

namespace blink {

class DOMArrayBuffer;
class EncodedAudioChunkInit;

class MODULES_EXPORT EncodedAudioChunk final : public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  EncodedAudioChunk(EncodedAudioMetadata metadata, DOMArrayBuffer* buffer);

  static EncodedAudioChunk* Create(const EncodedAudioChunkInit* init);

  // encoded_audio_chunk.idl implementation.
  String type() const;
  uint64_t timestamp() const;
  DOMArrayBuffer* data() const;

  void Trace(Visitor* visitor) const override {
    visitor->Trace(buffer_);
    ScriptWrappable::Trace(visitor);
  }

 private:
  EncodedAudioMetadata metadata_;
  Member<DOMArrayBuffer> buffer_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_ENCODED_AUDIO_CHUNK_H_
