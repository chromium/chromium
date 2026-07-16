// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_WEBRTC_VOICE_ISOLATION_VOICE_ISOLATION_COMPONENT_H_
#define MEDIA_WEBRTC_VOICE_ISOLATION_VOICE_ISOLATION_COMPONENT_H_

#include "base/component_export.h"
#include "base/containers/span.h"

namespace media {

class COMPONENT_EXPORT(MEDIA_WEBRTC) VoiceIsolationComponent {
 public:
  VoiceIsolationComponent() = default;
  virtual ~VoiceIsolationComponent();
  VoiceIsolationComponent(const VoiceIsolationComponent&) = delete;
  VoiceIsolationComponent& operator=(const VoiceIsolationComponent&) = delete;

  // Processes audio from the `input` span and writes the isolated voice audio
  // into the `output` span. Both spans must have exactly `FrameSize()`
  // elements.
  virtual void ProcessAudio(base::span<const float> input,
                            base::span<float> output) = 0;

  // Returns the exact number of samples that this component expects in the
  // `input` and `output` spans for a single call to ProcessAudio().
  virtual size_t FrameSize() const = 0;

  // Returns the exact number of frames per second this component needs to
  // process in real-time. This is the calling frequency. For raw audio signals
  // `FramesPerSecond()`*`FrameSize()` is equal to the sampling rate.
  virtual size_t FramesPerSecond() const = 0;
};
}  // namespace media

#endif  // MEDIA_WEBRTC_VOICE_ISOLATION_VOICE_ISOLATION_COMPONENT_H_
