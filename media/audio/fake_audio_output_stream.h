// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_FAKE_AUDIO_OUTPUT_STREAM_H_
#define MEDIA_AUDIO_FAKE_AUDIO_OUTPUT_STREAM_H_

#include <memory>

#include "base/macros.h"
#include "media/audio/android/muteable_audio_output_stream.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_parameters.h"
#include "media/base/fake_audio_worker.h"

namespace media {

class AudioManagerBase;

// A fake implementation of AudioOutputStream.  Used for testing and when a real
// audio output device is unavailable or refusing output (e.g. remote desktop).
// Callbacks are driven on the AudioManager's message loop.
class MEDIA_EXPORT FakeAudioOutputStream : public MuteableAudioOutputStream {
 public:
  static AudioOutputStream* MakeFakeStream(AudioManagerBase* manager,
                                           const AudioParameters& params);

  // AudioOutputStream implementation.
  bool Open() override;
  void Start(AudioSourceCallback* callback) override;
  void Stop() override;
  void SetVolume(double volume) override;
  void GetVolume(double* volume) override;
  void Close() override;
  void Flush() override;
  void SetMute(bool muted) override;

 private:
  FakeAudioOutputStream(AudioManagerBase* manager,
                        const AudioParameters& params);
  ~FakeAudioOutputStream() override;

  // Task that periodically calls OnMoreData() to consume audio data.
  void CallOnMoreData(base::TimeTicks ideal_time, base::TimeTicks now);

  AudioManagerBase* const audio_manager_;
  const base::TimeDelta fixed_data_delay_;
  AudioSourceCallback* callback_;
  FakeAudioWorker fake_worker_;
  const std::unique_ptr<AudioBus> audio_bus_;

  DISALLOW_COPY_AND_ASSIGN(FakeAudioOutputStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_FAKE_AUDIO_OUTPUT_STREAM_H_
