// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_MUTEABLE_H_
#define SERVICES_AUDIO_MUTEABLE_H_

namespace audio {
class Muteable {
 public:
  // Starts/Stops muting of the outbound audio signal from this group member.
  // However, the audio data being sent to Snoopers should be the original,
  // unmuted audio. Note that an equal number of start versus stop calls here is
  // not required, and the implementation should ignore redundant calls.
  virtual void StartMuting() = 0;
  virtual void StopMuting() = 0;

 protected:
  virtual ~Muteable() = default;
};
}  // namespace audio

#endif  // SERVICES_AUDIO_MUTEABLE_H_
