// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/audio_capturer_chromeos.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_features.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/sequence_checker.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/sequence_bound.h"
#include "media/audio/audio_manager.h"
#include "remoting/host/chromeos/audio_helper_chromeos.h"
#include "remoting/host/chromeos/audio_helper_chromeos_impl.h"
#include "remoting/proto/audio.pb.h"

namespace remoting {

AudioCapturerChromeOs::AudioCapturerChromeOs(
    std::unique_ptr<AudioHelperChromeOs> audio_helper_chromeos)
    : audio_helper_chromeos_(media::AudioManager::Get()->GetTaskRunner(),
                             std::move(audio_helper_chromeos)) {
  // Allow rebinding |sequence_checker_| to the sequence that `Start()` is
  // eventually called on.
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

AudioCapturerChromeOs::~AudioCapturerChromeOs() = default;

bool AudioCapturerChromeOs::Start(const PacketCapturedCallback& callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  packet_captured_callback_ = callback;

  // Post the StartAudioStream call to the audio thread, as all interactions
  // with AudioManager must happen on that thread. Using `BindPostTask()`, when
  // the AudioHelperChromeos executes the HandleAudioData and HandleAudioError
  // callbacks, it automatically posts those tasks back to this "current"
  // sequence.
  //
  // Note: AudioCapturerChromeOs currently runs on the main sequence. If we
  // observe performance issues like audio packet delays, we may need to
  // revisit this and move AudioCapturerChromeOs to its own higher-priority
  // thread.
  audio_helper_chromeos_.AsyncCall(&AudioHelperChromeOs::StartAudioStream)
      .WithArgs(
          audio_playback_mode_,
          base::BindPostTask(
              base::SequencedTaskRunner::GetCurrentDefault(),
              base::BindRepeating(&AudioCapturerChromeOs::HandleAudioData,
                                  weak_ptr_factory_.GetWeakPtr())),
          base::BindPostTask(
              base::SequencedTaskRunner::GetCurrentDefault(),
              base::BindRepeating(&AudioCapturerChromeOs::HandleAudioError,
                                  weak_ptr_factory_.GetWeakPtr())));

  return true;
}

void AudioCapturerChromeOs::SetAudioPlaybackMode(AudioPlaybackMode mode) {
  audio_playback_mode_ = mode;
}

void AudioCapturerChromeOs::HandleAudioData(
    std::unique_ptr<AudioPacket> packet) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (packet_captured_callback_) {
    packet_captured_callback_.Run(std::move(packet));
  }
}

void AudioCapturerChromeOs::HandleAudioError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  packet_captured_callback_.Reset();
}

// static
bool AudioCapturer::IsSupported() {
  return ash::features::IsBocaHostAudioEnabled();
}

// static
std::unique_ptr<AudioCapturer> AudioCapturer::Create() {
  if (!IsSupported()) {
    return nullptr;
  }

  return std::make_unique<AudioCapturerChromeOs>(
      std::make_unique<AudioHelperChromeOsImpl>());
}

}  // namespace remoting
