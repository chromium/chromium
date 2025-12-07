// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_AUDIO_CAPTURER_CHROMEOS_H_
#define REMOTING_HOST_AUDIO_CAPTURER_CHROMEOS_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/threading/sequence_bound.h"
#include "remoting/host/audio_capturer.h"
#include "remoting/host/chromeos/audio_helper_chromeos.h"

namespace remoting {

class AudioPacket;

class AudioCapturerChromeOs : public AudioCapturer {
 public:
  explicit AudioCapturerChromeOs(
      std::unique_ptr<AudioHelperChromeOs> audio_helper_chromeos);

  AudioCapturerChromeOs(const AudioCapturerChromeOs&) = delete;
  AudioCapturerChromeOs& operator=(const AudioCapturerChromeOs&) = delete;
  ~AudioCapturerChromeOs() override;

  // remoting::protocol::AudioSource:
  bool Start(const PacketCapturedCallback& callback) override;

  // remoting::AudioCapturer:
  void SetAudioPlaybackMode(AudioPlaybackMode mode) override;

 private:
  // These methods are called on the main thread by AudioHelperChromeOs (which
  // lives on the Audio thread).
  void HandleAudioData(std::unique_ptr<AudioPacket> packet);
  void HandleAudioError();

  SEQUENCE_CHECKER(sequence_checker_);

  PacketCapturedCallback packet_captured_callback_;

  AudioPlaybackMode audio_playback_mode_ = AudioPlaybackMode::kUnknown;

  // AudioHelperChromeOs interacts with AudioManager, which requires all its
  // methods to be called on the Audio thread. This ensures when the CRD session
  // is closed, and this class is being destroyed, AudioHelperChromeOs always
  // gets destroyed first. This prevents AudioHelperChromeOs from trying to
  // execute already deleted callbacks which would cause a crash.
  base::SequenceBound<std::unique_ptr<AudioHelperChromeOs>>
      audio_helper_chromeos_;

  // Needs to be last member variable.
  base::WeakPtrFactory<AudioCapturerChromeOs> weak_ptr_factory_{this};
};

}  // namespace remoting

#endif  // REMOTING_HOST_AUDIO_CAPTURER_CHROMEOS_H_
