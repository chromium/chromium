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
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "media/audio/audio_device_description.h"
#include "media/audio/audio_io.h"
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

constexpr char kAudioPlaybackModeHistogramName[] =
    "Remoting.Host.ChromeOs.AudioStream.AudioPlaybackMode";
constexpr char kAudioStreamErrorHistogramName[] =
    "Remoting.Host.ChromeOs.AudioStream.OnError";
constexpr char kAudioStreamOpenOutcomeHistogramName[] =
    "Remoting.Host.ChromeOs.AudioStream.OpenOutcome";
constexpr char kStartAudioStreamHistogramName[] =
    "Remoting.Host.ChromeOs.AudioStream.StartResult";

void RecordOpenOutcome(media::AudioInputStream::OpenOutcome open_outcome) {
  OpenOutcomeChromeOs open_outcome_chromeos;
  switch (open_outcome) {
    case media::AudioInputStream::OpenOutcome::kSuccess:
      open_outcome_chromeos = OpenOutcomeChromeOs::kSuccess;
      break;
    case media::AudioInputStream::OpenOutcome::kAlreadyOpen:
      open_outcome_chromeos = OpenOutcomeChromeOs::kAlreadyOpen;
      break;
    case media::AudioInputStream::OpenOutcome::kFailed:
      open_outcome_chromeos = OpenOutcomeChromeOs::kFailed;
      break;
    case media::AudioInputStream::OpenOutcome::kFailedSystemPermissions:
      open_outcome_chromeos = OpenOutcomeChromeOs::kFailedSystemPermissions;
      break;
    case media::AudioInputStream::OpenOutcome::kFailedInUse:
      open_outcome_chromeos = OpenOutcomeChromeOs::kFailedInUse;
      break;
  }
  base::UmaHistogramEnumeration(kAudioStreamOpenOutcomeHistogramName,
                                open_outcome_chromeos);
}

}  // namespace

AudioHelperChromeOsImpl::AudioHelperChromeOsImpl()
    : audio_runner_(media::AudioManager::Get()->GetTaskRunner()),
      audio_params_(media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
                    media::ChannelLayoutConfig::Stereo(),
                    kSampleRate,
                    kFramesPerBuffer) {}

AudioHelperChromeOsImpl::~AudioHelperChromeOsImpl() {
  StopAudioStream();
}

void AudioHelperChromeOsImpl::StartAudioStream(
    AudioPlaybackMode audio_playback_mode,
    OnDataCallback on_data_callback,
    OnErrorCallback on_error_callback) {
  DCHECK(audio_runner_->RunsTasksInCurrentSequence());
  base::UmaHistogramEnumeration(kAudioPlaybackModeHistogramName,
                                audio_playback_mode);

  if (stream_) {
    LOG(WARNING) << "Audio stream already started.";
    base::UmaHistogramEnumeration(
        kStartAudioStreamHistogramName,
        AudioHelperStartStreamResult::kStreamAlreadyStarted);
    return;
  }

  on_data_callback_ = std::move(on_data_callback);
  on_error_callback_ = std::move(on_error_callback);

  std::string device_id;
  switch (audio_playback_mode) {
    case AudioPlaybackMode::kRemoteAndLocal:
      device_id = media::AudioDeviceDescription::kDefaultDeviceId;
      break;
    case AudioPlaybackMode::kRemoteOnly:
      device_id = media::AudioDeviceDescription::kLoopbackWithMuteDeviceId;
      break;
    case AudioPlaybackMode::kLocalOnly:
    case AudioPlaybackMode::kUnknown:
      NOTREACHED()
          << "audio_helper should not be created when audio is not being "
             "remoted.";
  }
  stream_ = media::AudioManager::Get()->MakeAudioInputStream(
      audio_params_, device_id, base::BindRepeating([](const std::string& msg) {
        LOG(WARNING) << "Stream: " << msg;
      }));

  if (!stream_) {
    LOG(ERROR) << "Failed to create input stream.";
    NotifyFatalStreamError();
    base::UmaHistogramEnumeration(
        kStartAudioStreamHistogramName,
        AudioHelperStartStreamResult::kFailedToCreateStream);
    return;
  }

  media::AudioInputStream::OpenOutcome open_outcome = stream_->Open();
  RecordOpenOutcome(open_outcome);
  if (open_outcome != media::AudioInputStream::OpenOutcome::kSuccess) {
    LOG(ERROR) << "Failed to open stream.";
    stream_ = nullptr;
    NotifyFatalStreamError();
    base::UmaHistogramEnumeration(
        kStartAudioStreamHistogramName,
        AudioHelperStartStreamResult::kFailedToOpenStream);
    return;
  }

  stream_->Start(this);
  LOG(WARNING) << "Audio input stream successfully started.";
  base::UmaHistogramEnumeration(kStartAudioStreamHistogramName,
                                AudioHelperStartStreamResult::kSuccess);
}

void AudioHelperChromeOsImpl::StopAudioStream() {
  DCHECK(audio_runner_->RunsTasksInCurrentSequence());
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

  on_data_callback_.Run(std::move(packet));
}

void AudioHelperChromeOsImpl::OnError() {
  DCHECK(audio_runner_->RunsTasksInCurrentSequence());
  LOG(ERROR) << "AudioInputStream Error encountered.";
  base::UmaHistogramBoolean(kAudioStreamErrorHistogramName, /* sample= */ true);
  NotifyFatalStreamError();
}

void AudioHelperChromeOsImpl::NotifyFatalStreamError() {
  // Stop the current audio stream before notifying the audio capturer of the
  // failure.
  StopAudioStream();
  on_error_callback_.Run();
}

}  // namespace remoting
