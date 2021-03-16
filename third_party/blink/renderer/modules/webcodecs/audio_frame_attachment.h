// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_AUDIO_FRAME_ATTACHMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_AUDIO_FRAME_ATTACHMENT_H_

#include "base/optional.h"
#include "third_party/blink/renderer/bindings/core/v8/serialization/serialized_script_value.h"
#include "third_party/blink/renderer/modules/modules_export.h"

namespace blink {

class AudioFrameSerializationData;

// Used to serialize audio frames.
class MODULES_EXPORT AudioFrameAttachment
    : public SerializedScriptValue::Attachment {
 public:
  using SerializationDataVector =
      Vector<std::unique_ptr<AudioFrameSerializationData>>;

  static const void* const kAttachmentKey;
  AudioFrameAttachment() = default;
  ~AudioFrameAttachment() override = default;

  bool IsLockedToAgentCluster() const override {
    return !audio_serialization_data_.IsEmpty();
  }

  size_t size() const { return audio_serialization_data_.size(); }

  SerializationDataVector& SerializationData() {
    return audio_serialization_data_;
  }

  const SerializationDataVector& SerializationData() const {
    return audio_serialization_data_;
  }

 private:
  SerializationDataVector audio_serialization_data_;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_MODULES_WEBCODECS_VIDEO_FRAME_ATTACHMENT_H_
