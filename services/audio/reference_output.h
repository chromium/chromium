// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_REFERENCE_OUTPUT_H_
#define SERVICES_AUDIO_REFERENCE_OUTPUT_H_

#include "base/time/time.h"

namespace media {
class AudioBus;
}  // namespace media

namespace audio {
class ReferenceOutput {
 public:
  class Listener {
   public:
    // Provides read-only access to the auio played by ReferenceOutput.
    // Must execute quickly, as it will typically be called on a realtime
    // thread; otherwise, audio glitches may occur.
    virtual void OnPlayoutData(const media::AudioBus& audio_bus,
                               int sample_rate,
                               base::TimeDelta audio_delay) = 0;

   protected:
    virtual ~Listener() = default;
  };

  // Starts/Stops listening to the reference output.
  virtual void StartListening(Listener* listener) = 0;
  virtual void StopListening(Listener* listener) = 0;

 protected:
  virtual ~ReferenceOutput() = default;
};
}  // namespace audio

#endif  // SERVICES_AUDIO_REFERENCE_OUTPUT_H_
