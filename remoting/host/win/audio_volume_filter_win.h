// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_WIN_AUDIO_VOLUME_FILTER_WIN_H_
#define REMOTING_HOST_WIN_AUDIO_VOLUME_FILTER_WIN_H_

#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

#include "remoting/host/audio_volume_filter.h"

namespace remoting {

// An implementation of AudioVolumeFilter for Windows only.
class AudioVolumeFilterWin : public AudioVolumeFilter {
 public:
  explicit AudioVolumeFilterWin(int silence_threshold);
  ~AudioVolumeFilterWin() override;

  // Initializes |audio_volume_|. Returns false if Windows APIs fail.
  bool ActivateBy(IMMDevice* mm_device);

 protected:
  // Returns current audio level from |audio_volume_|. If the initialization
  // failed, this function returns 1.
  float GetAudioLevel() override;

 private:
  Microsoft::WRL::ComPtr<IAudioEndpointVolume> audio_volume_;
};

}  // namespace remoting

#endif  // REMOTING_HOST_WIN_AUDIO_VOLUME_FILTER_WIN_H_
