// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_SNOOPABLE_H_
#define SERVICES_AUDIO_SNOOPABLE_H_

#include "base/time/time.h"

namespace media {
class AudioBus;
class AudioParameters;
}  // namespace media

namespace audio {
class Snoopable {
 public:
  class Snooper {
   public:
    // Provides read-only access to the data flowing through a GroupMember. This
    // must execute quickly, as it will typically be called on a realtime
    // thread; otherwise, audio glitches may occur.
    virtual void OnData(const media::AudioBus& audio_bus,
                        base::TimeTicks reference_time,
                        double volume) = 0;

   protected:
    virtual ~Snooper() = default;
  };

  // Returns the audio parameters of the snoopable audio data. The parameters
  // must not change for the lifetime of this group member, but can be different
  // than those of other members.
  virtual const media::AudioParameters& GetAudioParameters() const = 0;

  // Starts/Stops snooping on the audio data flowing through this group member.
  virtual void StartSnooping(Snooper* snooper) = 0;
  virtual void StopSnooping(Snooper* snooper) = 0;

 protected:
  virtual ~Snoopable() = default;
};
}  // namespace audio

#endif  // SERVICES_AUDIO_SNOOPABLE_H_
