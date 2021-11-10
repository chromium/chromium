// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/fuchsia/audio_input_stream_fuchsia.h"

#include <lib/sys/cpp/component_context.h>

#include "base/fuchsia/process_context.h"
#include "base/logging.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/fuchsia/audio_manager_fuchsia.h"

namespace media {

class AudioInputStreamFuchsia::CaptureCallbackAdapter
    : public AudioCapturerSource::CaptureCallback {
 public:
  CaptureCallbackAdapter(AudioInputCallback* callback) : callback_(callback) {}

  void Capture(const AudioBus* audio_source,
               base::TimeTicks audio_capture_time,
               double volume,
               bool key_pressed) override {
    callback_->OnData(audio_source, audio_capture_time, volume);
  }

  void OnCaptureError(AudioCapturerSource::ErrorCode code,
                      const std::string& message) override {
    callback_->OnError();
  }

  void OnCaptureMuted(bool is_muted) override {}

 private:
  AudioInputCallback* callback_;
};

AudioInputStreamFuchsia::AudioInputStreamFuchsia(
    AudioManagerFuchsia* manager,
    const AudioParameters& parameters,
    std::string device_id)
    : manager_(manager),
      parameters_(parameters),
      device_id_(std::move(device_id)) {
  DCHECK(device_id_.empty() ||
         device_id_ == AudioDeviceDescription::kLoopbackInputDeviceId ||
         device_id_ == AudioDeviceDescription::kDefaultDeviceId)
      << "AudioInput from " << device_id_ << " not implemented!";
}

AudioInputStreamFuchsia::~AudioInputStreamFuchsia() = default;

AudioInputStream::OpenOutcome AudioInputStreamFuchsia::Open() {
  return OpenOutcome::kSuccess;
}

void AudioInputStreamFuchsia::Start(AudioInputCallback* callback) {
  fidl::InterfaceHandle<fuchsia::media::AudioCapturer> capturer;
  auto factory = base::ComponentContextForProcess()
                     ->svc()
                     ->Connect<fuchsia::media::Audio>();
  bool is_loopback =
      device_id_ == AudioDeviceDescription::kLoopbackInputDeviceId;
  factory->CreateAudioCapturer(capturer.NewRequest(), is_loopback);

  capturer_source_ = base::MakeRefCounted<FuchsiaAudioCapturerSource>(
      std::move(capturer), manager_->GetTaskRunner());
  callback_adapter_ = std::make_unique<CaptureCallbackAdapter>(callback);
  capturer_source_->Initialize(parameters_, callback_adapter_.get());
  capturer_source_->Start();
}

void AudioInputStreamFuchsia::Stop() {
  if (capturer_source_) {
    capturer_source_->Stop();
    capturer_source_ = nullptr;
  }
}

void AudioInputStreamFuchsia::Close() {
  Stop();
  manager_->ReleaseInputStream(this);
}

double AudioInputStreamFuchsia::GetMaxVolume() {
  return 1.0;
}

void AudioInputStreamFuchsia::SetVolume(double volume) {
  capturer_source_->SetVolume(volume);
  volume_ = volume;
}

double AudioInputStreamFuchsia::GetVolume() {
  return volume_;
}

bool AudioInputStreamFuchsia::SetAutomaticGainControl(bool enabled) {
  capturer_source_->SetAutomaticGainControl(enabled);
  automatic_gain_control_ = enabled;
  return true;
}

bool AudioInputStreamFuchsia::GetAutomaticGainControl() {
  return automatic_gain_control_;
}

bool AudioInputStreamFuchsia::IsMuted() {
  return volume_ == 0.0;
}

void AudioInputStreamFuchsia::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  capturer_source_->SetOutputDeviceForAec(output_device_id);
}

}  // namespace media
