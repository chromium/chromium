// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_FUCHSIA_AUDIO_INPUT_STREAM_FUCHSIA_H_
#define MEDIA_AUDIO_FUCHSIA_AUDIO_INPUT_STREAM_FUCHSIA_H_

#include "media/audio/audio_io.h"
#include "media/base/audio_parameters.h"
#include "media/fuchsia/audio/fuchsia_audio_capturer_source.h"

namespace media {

class AudioManagerFuchsia;

class AudioInputStreamFuchsia : public AudioInputStream {
 public:
  // Caller must ensure that manager outlives the stream.
  AudioInputStreamFuchsia(AudioManagerFuchsia* manager,
                          const AudioParameters& parameters,
                          std::string device_id);

  OpenOutcome Open() override;
  void Start(AudioInputCallback* callback) override;
  void Stop() override;
  void Close() override;
  double GetMaxVolume() override;
  void SetVolume(double volume) override;
  double GetVolume() override;
  bool SetAutomaticGainControl(bool enabled) override;
  bool GetAutomaticGainControl() override;
  bool IsMuted() override;
  void SetOutputDeviceForAec(const std::string& output_device_id) override;

 private:
  class CaptureCallbackAdapter;

  ~AudioInputStreamFuchsia() override;

  AudioManagerFuchsia* const manager_;
  AudioParameters parameters_;
  std::string device_id_;
  std::unique_ptr<CaptureCallbackAdapter> callback_adapter_;
  scoped_refptr<FuchsiaAudioCapturerSource> capturer_source_;
  double volume_ = 1.0;
  bool automatic_gain_control_ = false;
};

}  // namespace media

#endif  // MEDIA_AUDIO_FUCHSIA_AUDIO_INPUT_STREAM_FUCHSIA_H_
