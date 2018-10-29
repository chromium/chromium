// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/mock_audio_renderer_sink.h"

namespace media {
MockAudioRendererSink::MockAudioRendererSink()
    : MockAudioRendererSink(OUTPUT_DEVICE_STATUS_OK) {}

MockAudioRendererSink::MockAudioRendererSink(OutputDeviceStatus device_status)
    : MockAudioRendererSink(std::string(), device_status) {}

MockAudioRendererSink::MockAudioRendererSink(const std::string& device_id,
                                             OutputDeviceStatus device_status)
    : MockAudioRendererSink(
          device_id,
          device_status,
          AudioParameters(AudioParameters::AUDIO_FAKE,
                          CHANNEL_LAYOUT_STEREO,
                          AudioParameters::kTelephoneSampleRate,
                          1)) {}

MockAudioRendererSink::MockAudioRendererSink(
    const std::string& device_id,
    OutputDeviceStatus device_status,
    const AudioParameters& device_output_params)
    : output_device_info_(device_id, device_status, device_output_params) {}

MockAudioRendererSink::~MockAudioRendererSink() = default;

void MockAudioRendererSink::SwitchOutputDevice(const std::string& device_id,
                                               OutputDeviceStatusCB callback) {
  // NB: output device won't be changed, since it's not required by any tests
  // now.
  std::move(callback).Run(output_device_info_.device_status());
}

void MockAudioRendererSink::Initialize(const AudioParameters& params,
                                       RenderCallback* renderer) {
  callback_ = renderer;
}

OutputDeviceInfo MockAudioRendererSink::GetOutputDeviceInfo() {
  return output_device_info_;
}

bool MockAudioRendererSink::IsOptimizedForHardwareParameters() {
  return false;
}

}  // namespace media
