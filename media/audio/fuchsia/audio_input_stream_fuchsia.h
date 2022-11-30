// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_FUCHSIA_AUDIO_INPUT_STREAM_FUCHSIA_H_
#define MEDIA_AUDIO_FUCHSIA_AUDIO_INPUT_STREAM_FUCHSIA_H_

#include <fuchsia/media/cpp/fidl.h>

#include "media/audio/audio_io.h"
#include "media/base/audio_parameters.h"
#include "media/base/media_export.h"
#include "media/fuchsia/common/vmo_buffer.h"

namespace media {

class AudioManagerFuchsia;

class MEDIA_EXPORT AudioInputStreamFuchsia : public AudioInputStream {
 public:
  // Caller must ensure that manager outlives the stream.
  AudioInputStreamFuchsia(AudioManagerFuchsia* manager,
                          const AudioParameters& parameters,
                          std::string device_id);
  ~AudioInputStreamFuchsia() override;

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
  // OnPacketProduced event handler for the |capturer_|.
  void OnPacketProduced(fuchsia::media::StreamPacket packet);

  // Reports an error to |callback_| and disconnects |capturer_|.
  void ReportError();

  AudioManagerFuchsia* const manager_;
  AudioParameters parameters_;
  std::string device_id_;

  fuchsia::media::AudioCapturerPtr capturer_;

  // VMO with the AudioCapturer in order to pass the captured data.
  VmoBuffer capture_buffer_;

  // Indicates that async capture mode has been activated for |capturer_|, i.e.
  // StartAsyncCapture() has been called.
  bool is_capturer_started_ = false;

  std::unique_ptr<AudioBus> audio_bus_;

  AudioInputCallback* callback_ = nullptr;
};

}  // namespace media

#endif  // MEDIA_AUDIO_FUCHSIA_AUDIO_INPUT_STREAM_FUCHSIA_H_
