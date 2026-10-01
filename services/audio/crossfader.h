// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_CROSSFADER_H_
#define SERVICES_AUDIO_CROSSFADER_H_

#include "base/containers/heap_array.h"

namespace media {
class AudioBus;
}  // namespace media

namespace audio {

// Crossfades media::AudioBus buffers within a single buffer, using a
// precomputed rising half-Hann (raised-cosine) window of N gains:
//   w[n] = sin^2(pi * (n + 1) / (2 * N)),  0 <= n < N.
// The last gain is exactly 1.0, so audio following the faded buffer continues
// without a step. The matching fade-out gain is 1 - w[n].
//
// The constructor allocates. Crossfade() does not allocate or lock and is
// safe to call on a real-time audio thread. The object is immutable
// after construction, so it can be used from any thread.
class Crossfader {
 public:
  // `frames_per_buffer` is N, the length of every bus passed to this object.
  explicit Crossfader(int frames_per_buffer);

  Crossfader(const Crossfader&) = delete;
  Crossfader& operator=(const Crossfader&) = delete;

  ~Crossfader();

  // Writes a crossfade from `from` (fading out) to `to` (fading in):
  //   destination[n] = (1 - w[n]) * from[n] + w[n] * to[n].
  // `destination` may alias `from` or `to`, since every sample is read before
  // it is written.
  void Crossfade(const media::AudioBus& from,
                 const media::AudioBus& to,
                 media::AudioBus& destination) const;

 private:
  const base::HeapArray<float> fade_in_window_;
};

}  // namespace audio

#endif  // SERVICES_AUDIO_CROSSFADER_H_
