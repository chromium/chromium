// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_AUDIO_FRAME_ATTACHMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_AUDIO_FRAME_ATTACHMENT_H_

#include "base/optional.h"
#include "media/base/audio_buffer.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

// Used to serialize audio frames.
class MODULES_EXPORT AudioFrameAttachment
    : public SerializedScriptValue::Attachment {
 public:
  using AudioBufferVector = Vector<scoped_refptr<media::AudioBuffer>>;

  static const void* const kAttachmentKey;
  AudioFrameAttachment() = default;
  ~AudioFrameAttachment() override = default;

  bool IsLockedToAgentCluster() const override {
    return !audio_buffers_.IsEmpty();
  }

  size_t size() const { return audio_buffers_.size(); }

  AudioBufferVector& AudioBuffers() { return audio_buffers_; }

  const AudioBufferVector& AudioBuffers() const { return audio_buffers_; }

 private:
  AudioBufferVector audio_buffers_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_AUDIO_FRAME_ATTACHMENT_H_
