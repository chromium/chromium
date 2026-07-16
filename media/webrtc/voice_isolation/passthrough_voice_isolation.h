// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBRTC_VOICE_ISOLATION_PASSTHROUGH_VOICE_ISOLATION_H_
#define MEDIA_WEBRTC_VOICE_ISOLATION_PASSTHROUGH_VOICE_ISOLATION_H_

#include "base/component_export.h"
#include "base/containers/span.h"
#include "media/webrtc/voice_isolation/voice_isolation_component.h"

namespace media {

class VoiceIsolationComponent;

// Copies the input to the output without changing it.
class COMPONENT_EXPORT(MEDIA_WEBRTC) PassthroughVoiceIsolation
    : public VoiceIsolationComponent {
 public:
  PassthroughVoiceIsolation(size_t frame_size, size_t frames_per_second);

  void ProcessAudio(base::span<const float> input,
                    base::span<float> output) override;

  size_t FrameSize() const override;

  size_t FramesPerSecond() const override;

 private:
  const size_t frame_size_;
  const size_t frames_per_second_;
};

}  // namespace media
#endif  // MEDIA_WEBRTC_VOICE_ISOLATION_PASSTHROUGH_VOICE_ISOLATION_H_
