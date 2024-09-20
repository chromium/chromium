// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/video_encoder_buffer.h"

#include <string>

#include "third_party/blink/renderer/modules/webcodecs/video_encoder.h"

namespace blink {

VideoEncoderBuffer::VideoEncoderBuffer(VideoEncoder* owner, size_t id)
    : id_(id), owner_(owner) {}

String VideoEncoderBuffer::id() const {
  return String::Format("%zu", id_);
}

void VideoEncoderBuffer::Trace(Visitor* visitor) const {
  visitor->Trace(owner_);
  ScriptWrappable::Trace(visitor);
}

}  // namespace blink
