// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/audio/test/fake_loopback_group_member.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "base/numerics/math_constants.h"
#include "media/audio/audio_device_description.h"
#include "media/base/audio_bus.h"

namespace audio {

FakeLoopbackGroupMember::FakeLoopbackGroupMember(
    const media::AudioParameters& params)
    : params_(params),
      audio_bus_(media::AudioBus::Create(params_)),
      frequency_by_channel_(params_.channels(), 0.0) {
  CHECK(params_.IsValid());
}

FakeLoopbackGroupMember::~FakeLoopbackGroupMember() = default;

void FakeLoopbackGroupMember::SetChannelTone(int ch, double frequency) {
  if (ch == kSetAllChannels) {
    for (double& channel_frequency : frequency_by_channel_) {
      channel_frequency = frequency;
    }
  } else {
    CHECK_LE(0, ch);
    CHECK_LT(ch, params_.channels());
    frequency_by_channel_[ch] = frequency;
  }
}

void FakeLoopbackGroupMember::SetVolume(double volume) {
  CHECK_GE(volume, 0.0);
  CHECK_LE(volume, 1.0);
  volume_ = volume;
}

void FakeLoopbackGroupMember::RenderMoreAudio(
    base::TimeTicks output_timestamp) {
  if (snooper_) {
    for (int ch = 0; ch < params_.channels(); ++ch) {
      const double step = 2.0 * base::kPiDouble * frequency_by_channel_[ch] /
                          params_.sample_rate();
      float* const samples = audio_bus_->channel(ch);
      for (int frame = 0; frame < params_.frames_per_buffer(); ++frame) {
        samples[frame] = std::sin((at_frame_ + frame) * step);
      }
    }
    snooper_->OnData(*audio_bus_, output_timestamp, volume_);
  }
  at_frame_ += params_.frames_per_buffer();
}

const media::AudioParameters& FakeLoopbackGroupMember::GetAudioParameters()
    const {
  return params_;
}

std::string FakeLoopbackGroupMember::GetDeviceId() const {
  return media::AudioDeviceDescription::kDefaultDeviceId;
}

void FakeLoopbackGroupMember::StartSnooping(Snooper* snooper) {
  CHECK(!snooper_);
  snooper_ = snooper;
}

void FakeLoopbackGroupMember::StopSnooping(Snooper* snooper) {
  snooper_ = nullptr;
}

void FakeLoopbackGroupMember::StartMuting() {
  // No effect for this fake implementation.
}

void FakeLoopbackGroupMember::StopMuting() {
  // No effect for this fake implementation.
}

}  // namespace audio
