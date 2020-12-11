// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_AUDIO_INPUT_STREAM_DATA_INTERCEPTOR_H_
#define MEDIA_AUDIO_AUDIO_INPUT_STREAM_DATA_INTERCEPTOR_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "media/audio/audio_io.h"
#include "media/base/media_export.h"

namespace media {

class AudioDebugRecorder;

// This class wraps an AudioInputStream to be able to intercerpt the data for
// debug recording purposes.
class MEDIA_EXPORT AudioInputStreamDataInterceptor
    : public AudioInputStream,
      public AudioInputStream::AudioInputCallback {
 public:
  using CreateDebugRecorderCB =
      base::RepeatingCallback<std::unique_ptr<AudioDebugRecorder>()>;

  // |stream| is the stream that this object should forward all stream
  // operations to. It will intercept OnData callbacks and send the audio data
  // to the debug recorder created by |create_debug_recorder_cb|.
  AudioInputStreamDataInterceptor(
      CreateDebugRecorderCB create_debug_recorder_cb,
      AudioInputStream* stream);

  ~AudioInputStreamDataInterceptor() override;

  // Implementation of AudioInputStream.
  bool Open() override;
  void Start(AudioInputStream::AudioInputCallback* callback) override;
  void Stop() override;
  void Close() override;
  double GetMaxVolume() override;
  void SetVolume(double volume) override;
  double GetVolume() override;
  bool IsMuted() override;
  bool SetAutomaticGainControl(bool enabled) override;
  bool GetAutomaticGainControl() override;
  void SetOutputDeviceForAec(const std::string& output_device_id) override;

  // Implementation of AudioInputCallback
  void OnData(const AudioBus* source,
              base::TimeTicks capture_time,
              double volume) override;

  void OnError() override;

 private:
  const CreateDebugRecorderCB create_debug_recorder_cb_;
  std::unique_ptr<AudioDebugRecorder> debug_recorder_;
  AudioInputStream* const stream_;
  AudioInputStream::AudioInputCallback* callback_;
  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(AudioInputStreamDataInterceptor);
};

}  // namespace media

#endif  // MEDIA_AUDIO_AUDIO_INPUT_STREAM_DATA_INTERCEPTOR_H_
