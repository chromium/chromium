// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/chromeos/audio_helper_chromeos_impl.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_manager.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_glitch_info.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/mojom/audio_data.mojom.h"
#include "remoting/proto/audio.pb.h"

namespace remoting {

namespace {

// Each buffer will contain 10 milliseconds of audio data.
// 48000 samples per second / 100 = 480 samples per 10ms.
constexpr int kSampleRate = 48000;
constexpr int kFramesPerBuffer = kSampleRate / 100;

}  // namespace

AudioHelperChromeOsImpl::AudioHelperChromeOsImpl()
    : audio_params_(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                    media::ChannelLayoutConfig::Stereo(),
                    kSampleRate,
                    kFramesPerBuffer) {}

AudioHelperChromeOsImpl::~AudioHelperChromeOsImpl() {
  StopAudioStream();
}

void AudioHelperChromeOsImpl::StartAudioStream(
    scoped_refptr<base::SequencedTaskRunner> main_task_runner,
    OnDataCallback on_data_callback,
    OnErrorCallback on_error_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(crbug.com/450048643): Figure out error handling
  if (stream_) {
    LOG(WARNING) << "Audio stream already started.";
    return;
  }

  main_task_runner_ = std::move(main_task_runner);
  on_data_callback_ = std::move(on_data_callback);
  on_error_callback_ = std::move(on_error_callback);

  // TODO(crbug.com/450048829): Choose the correct device id based on
  // application. Currently this mutes the host device's audio.
  std::string device_id =
      media::AudioDeviceDescription::kLoopbackWithMuteDeviceId;
  stream_ = media::AudioManager::Get()->MakeAudioInputStream(
      audio_params_, device_id, base::BindRepeating([](const std::string& msg) {
        LOG(WARNING) << "Stream: " << msg;
      }));

  // TODO(crbug.com/450048643): Figure out error handling
  if (!stream_) {
    LOG(ERROR) << "Failed to create input stream.";
    ReportError();
    return;
  }

  // TODO(crbug.com/450048643): Figure out error handling
  if (stream_->Open() != media::AudioInputStream::OpenOutcome::kSuccess) {
    LOG(ERROR) << "Failed to open stream.";
    stream_ = nullptr;
    ReportError();
    return;
  }

  stream_->Start(this);
}

void AudioHelperChromeOsImpl::StopAudioStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (stream_) {
    stream_->Stop();
    stream_->Close();
    stream_ = nullptr;
  }
  first_capture_time_ = std::nullopt;
}

void AudioHelperChromeOsImpl::OnData(
    const media::AudioBus* audio_bus,
    base::TimeTicks capture_time,
    double volume,
    const media::AudioGlitchInfo& glitch_info) {
  if (!first_capture_time_.has_value()) {
    first_capture_time_ = capture_time;
  }

  media::mojom::AudioDataS16Ptr audio_data_s16 =
      s16_converter_.ConvertToAudioDataS16(
          *audio_bus, audio_params_.sample_rate(),
          audio_params_.channel_layout(), true);

  auto packet = std::make_unique<AudioPacket>();
  const std::vector<int16_t>& data = audio_data_s16->data;
  packet->add_data(reinterpret_cast<const char*>(data.data()),
                   data.size() * sizeof(int16_t));
  packet->set_encoding(AudioPacket::ENCODING_RAW);
  packet->set_sampling_rate(AudioPacket::SAMPLING_RATE_48000);
  packet->set_bytes_per_sample(AudioPacket::BYTES_PER_SAMPLE_2);
  packet->set_channels(
      static_cast<AudioPacket::Channels>(audio_params_.channels()));
  packet->set_timestamp(
      (capture_time - first_capture_time_.value()).InMilliseconds());

  // Post the audio packet back to AudioCapturerChromeOs on the main sequence
  // via the callback.
  main_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(on_data_callback_, std::move(packet)));
}

void AudioHelperChromeOsImpl::OnError() {
  LOG(ERROR) << "AudioInputStream Error encountered.";
  ReportError();
}

void AudioHelperChromeOsImpl::ReportError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  main_task_runner_->PostTask(FROM_HERE, on_error_callback_);
}

}  // namespace remoting
