// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_AUDIO_VOLUME_FILTER_H_
#define REMOTING_HOST_AUDIO_VOLUME_FILTER_H_

#include "remoting/host/audio_silence_detector.h"

namespace remoting {

// A component to modify input audio sample to apply the audio level. This class
// is used on platforms which returns non-adjusted audio samples, e.g. Windows.
// This class supports frames with 16 bits per sample only.
class AudioVolumeFilter {
 public:
  // See AudioSilenceDetector for the meaning of |silence_threshold|.
  explicit AudioVolumeFilter(int silence_threshold);
  virtual ~AudioVolumeFilter();

  // Adjusts audio samples in |data|. If the samples are silent before applying
  // the volume level or the GetAudioLevel() returns 0, this function returns
  // false. If |frames| is 0, this function also returns false.
  bool Apply(int16_t* data, size_t frames);

  // Updates the sampling rate and channels.
  void Initialize(int sampling_rate, int channels);

 protected:
  // Returns the volume level in [0, 1]. This should be a normalized scalar
  // value: sample values can be simply multiplied by the result of this
  // function to apply volume.
  virtual float GetAudioLevel() = 0;

 private:
  AudioSilenceDetector silence_detector_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_AUDIO_VOLUME_FILTER_H_
