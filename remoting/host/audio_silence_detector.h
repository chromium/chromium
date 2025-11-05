// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_AUDIO_SILENCE_DETECTOR_H_
#define REMOTING_HOST_AUDIO_SILENCE_DETECTOR_H_

#include <stddef.h>
#include <stdint.h>

#include "base/containers/span.h"

namespace remoting {

// Helper used in audio capturers to detect and drop silent audio packets.
class AudioSilenceDetector {
 public:
  // |threshold| is used to specify maximum absolute sample value that should
  // still be considered as silence.
  explicit AudioSilenceDetector(int threshold);
  ~AudioSilenceDetector();

  void Reset(int sampling_rate, int channels);

  // Must be called for each new chunk of data. The `samples` span should
  // contain `frames * channels` total samples. Returns true if the given packet
  // is considered silence and should be dropped.
  bool IsSilence(base::span<const unsigned char> samples);

  // The count of channels received from last Reset().
  int channels() const;

 private:
  // Maximum absolute sample value that should still be considered as silence.
  int threshold_;

  // Silence period threshold in samples. Silence intervals shorter than this
  // value are still encoded and sent to the client, so that we don't disrupt
  // playback by dropping them.
  int silence_length_max_;

  // Lengths of the current silence period in samples.
  int silence_length_;

  // The count of channels.
  int channels_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_AUDIO_SILENCE_DETECTOR_H_
