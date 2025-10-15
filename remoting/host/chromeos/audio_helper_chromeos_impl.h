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

class AudioHelperChromeOsImpl
    : public AudioHelperChromeOs,
      public media::AudioInputStream::AudioInputCallback {
 public:
  AudioHelperChromeOsImpl();
  AudioHelperChromeOsImpl(const AudioHelperChromeOsImpl&) = delete;
  AudioHelperChromeOsImpl& operator=(const AudioHelperChromeOsImpl&) = delete;
  ~AudioHelperChromeOsImpl() override;

  // AudioHelperChromeOs:
  void StartAudioStream(
      scoped_refptr<base::SequencedTaskRunner> main_task_runner,
      OnDataCallback on_data_callback,
      OnErrorCallback on_error_callback) override;

 private:
  // media::AudioInputStream::AudioInputCallback:
  void OnData(const media::AudioBus* audio_bus,
              base::TimeTicks capture_time,
              double volume,
              const media::AudioGlitchInfo& glitch_info) override;
  void OnError() override;

  void StopAudioStream() override;
  void ReportError();

  // Task runner for the main sequence (where AudioCapturerChromeOs lives).
  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;
  OnDataCallback on_data_callback_;
  OnErrorCallback on_error_callback_;
  const media::AudioParameters audio_params_;

  raw_ptr<media::AudioInputStream> stream_ = nullptr;
  media::AudioDataS16Converter s16_converter_;
  std::optional<base::TimeTicks> first_capture_time_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace remoting

#endif  // REMOTING_HOST_CHROMEOS_AUDIO_HELPER_CHROMEOS_IMPL_H_
