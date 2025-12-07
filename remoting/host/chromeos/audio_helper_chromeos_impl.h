// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_CHROMEOS_AUDIO_HELPER_CHROMEOS_IMPL_H_
#define REMOTING_HOST_CHROMEOS_AUDIO_HELPER_CHROMEOS_IMPL_H_

#include <optional>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_parameters.h"
#include "media/mojo/common/audio_data_s16_converter.h"
#include "remoting/host/chromeos/audio_helper_chromeos.h"

namespace media {
class AudioBus;
struct AudioGlitchInfo;
}  // namespace media

namespace remoting {

// The enum below is used in histograms, do not remove/renumber entries. If
// you're adding to these enum, update the corresponding enum listing in
// tools/metrics/histograms/metadata/remoting/enums.xml.
enum class AudioHelperStartStreamResult {
  kSuccess = 0,
  kStreamAlreadyStarted = 1,
  kFailedToCreateStream = 2,
  kFailedToOpenStream = 3,
  kMaxValue = kFailedToOpenStream,
};

// The enum below maps to `OpenOutcome` in "media/audio/audio_io.h" and is used
// in histograms, do not remove/renumber entries. If you're adding to these
// enum, update the corresponding enum listing in
// tools/metrics/histograms/metadata/remoting/enums.xml.
enum class OpenOutcomeChromeOs {
  kSuccess = 0,
  kAlreadyOpen = 1,
  kFailed = 2,
  kFailedSystemPermissions = 3,
  kFailedInUse = 4,
  kMaxValue = kFailedInUse,
};

class AudioHelperChromeOsImpl
    : public AudioHelperChromeOs,
      public media::AudioInputStream::AudioInputCallback {
 public:
  AudioHelperChromeOsImpl();
  AudioHelperChromeOsImpl(const AudioHelperChromeOsImpl&) = delete;
  AudioHelperChromeOsImpl& operator=(const AudioHelperChromeOsImpl&) = delete;
  ~AudioHelperChromeOsImpl() override;

  // AudioHelperChromeOs:
  void StartAudioStream(AudioPlaybackMode audio_playback_mode,
                        OnDataCallback on_data_callback,
                        OnErrorCallback on_error_callback) override;

 private:
  // media::AudioInputStream::AudioInputCallback:
  void OnData(const media::AudioBus* audio_bus,
              base::TimeTicks capture_time,
              double volume,
              const media::AudioGlitchInfo& glitch_info) override;
  void OnError() override;

  void StopAudioStream();
  // When a fatal error occurs this stops the stream then invokes the
  // `on_error_callback_`.
  void NotifyFatalStreamError();

  const scoped_refptr<base::SequencedTaskRunner> audio_runner_;
  OnDataCallback on_data_callback_;
  OnErrorCallback on_error_callback_;
  const media::AudioParameters audio_params_;

  raw_ptr<media::AudioInputStream> stream_ = nullptr;
  media::AudioDataS16Converter s16_converter_;
  std::optional<base::TimeTicks> first_capture_time_;

};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMEOS_AUDIO_HELPER_CHROMEOS_IMPL_H_
