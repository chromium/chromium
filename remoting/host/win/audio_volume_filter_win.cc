// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/win/audio_volume_filter_win.h"

#include "base/check.h"
#include "base/logging.h"

namespace remoting {

AudioVolumeFilterWin::AudioVolumeFilterWin(int silence_threshold)
    : AudioVolumeFilter(silence_threshold) {}
AudioVolumeFilterWin::~AudioVolumeFilterWin() = default;

bool AudioVolumeFilterWin::ActivateBy(IMMDevice* mm_device) {
  DCHECK(mm_device);
  audio_volume_.Reset();
  // TODO(zijiehe): Do we need to control the volume per process?
  HRESULT hr = mm_device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL,
                                   nullptr, &audio_volume_);
  if (FAILED(hr)) {
    LOG(WARNING) << "Failed to get an IAudioEndpointVolume. Error " << hr;
    return false;
  }
  return true;
}

float AudioVolumeFilterWin::GetAudioLevel() {
  if (!audio_volume_) {
    return 1;
  }

  BOOL mute;
  HRESULT hr = audio_volume_->GetMute(&mute);
  if (FAILED(hr)) {
    LOG(ERROR) << "Failed to get mute status from IAudioEndpointVolume, error "
               << hr;
    return 1;
  }
  if (mute) {
    return 0;
  }

  float level;
  hr = audio_volume_->GetMasterVolumeLevelScalar(&level);
  if (FAILED(hr) || level > 1) {
    LOG(ERROR) << "Failed to get volume level from IAudioEndpointVolume, "
                  "error "
               << hr;
    return 1;
  }
  if (level < 0) {
    return 0;
  }
  return level;
}

}  // namespace remoting
