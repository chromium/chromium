// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_AUDIO_PUBLIC_CPP_SOUNDS_AUDIO_STREAM_HANDLER_H_
#define SERVICES_AUDIO_PUBLIC_CPP_SOUNDS_AUDIO_STREAM_HANDLER_H_

#include <stddef.h>
#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/media_export.h"
#include "services/service_manager/public/cpp/connector.h"

namespace audio {

// This class sends a sound to the audio output device.
class AudioStreamHandler {
 public:
  class TestObserver {
   public:
    virtual ~TestObserver() {}

    // Following methods will be called only from the audio thread.

    // Called when AudioStreamContainer was successfully created.
    virtual void Initialize(media::AudioRendererSink::RenderCallback* callback,
                            media::AudioParameters params) = 0;

    // Called when AudioOutputStreamProxy::Start() was successfully called.
    virtual void OnPlay() = 0;

    // Called when AudioOutputStreamProxy::Stop() was successfully called.
    virtual void OnStop(size_t cursor) = 0;
  };

  // C-tor for AudioStreamHandler. |wav_data| should be a raw
  // uncompressed WAVE data which will be sent to the audio output device.
  explicit AudioStreamHandler(
      std::unique_ptr<service_manager::Connector> connector,
      const base::StringPiece& wav_data);
  virtual ~AudioStreamHandler();

  // Returns true iff AudioStreamHandler is correctly initialized;
  bool IsInitialized() const;

  // Plays sound.  Volume level will be set according to current settings
  // and won't be changed during playback. Returns true iff new playback
  // was successfully started.
  //
  // NOTE: if current playback isn't at end of stream, playback request
  // is dropped, but true is returned.
  bool Play();

  // Stops current playback.
  void Stop();

  // Get the duration of the WAV data passed in.
  base::TimeDelta duration() const;

  static void SetObserverForTesting(TestObserver* observer);

 private:
  friend class AudioStreamHandlerTest;
  friend class SoundsManagerTest;

  class AudioStreamContainer;

  base::TimeDelta duration_;
  std::unique_ptr<AudioStreamContainer> stream_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  DISALLOW_COPY_AND_ASSIGN(AudioStreamHandler);
};

}  // namespace audio

#endif  // SERVICES_AUDIO_PUBLIC_CPP_SOUNDS_AUDIO_STREAM_HANDLER_H_
