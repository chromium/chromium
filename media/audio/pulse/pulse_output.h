// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Creates an audio output stream based on the PulseAudio asynchronous API;
// specifically using the pa_threaded_mainloop model.
//
// If the stream is successfully opened, Close() must be called before the
// stream is deleted as Close() is responsible for ensuring resource cleanup
// occurs.
//
// This object is designed so that all AudioOutputStream methods will be called
// on the same thread that created the object.
//
// WARNING: This object blocks on internal PulseAudio calls in Open() while
// waiting for PulseAudio's context structure to be ready.  It also blocks in
// inside PulseAudio in Start() and repeated during playback, waiting for
// PulseAudio write callbacks to occur.

#ifndef MEDIA_AUDIO_PULSE_PULSE_OUTPUT_H_
#define MEDIA_AUDIO_PULSE_PULSE_OUTPUT_H_

#include <stddef.h>

#include <memory>
#include <string>

#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_parameters.h"

struct pa_context;
struct pa_stream;
struct pa_threaded_mainloop;

namespace media {
class AudioManagerBase;

class PulseAudioOutputStream : public AudioOutputStream {
 public:
  PulseAudioOutputStream(const AudioParameters& params,
                         const std::string& device_id,
                         AudioManagerBase* manager);

  ~PulseAudioOutputStream() override;

  // Implementation of AudioOutputStream.
  bool Open() override;
  void Close() override;
  void Flush() override;
  void Start(AudioSourceCallback* callback) override;
  void Stop() override;
  void SetVolume(double volume) override;
  void GetVolume(double* volume) override;

 private:
  // Called by PulseAudio when |pa_stream_| change state.  If an unexpected
  // failure state change happens and |source_callback_| is set
  // this method will forward the error via OnError().
  static void StreamNotifyCallback(pa_stream* s, void* p_this);

  // Called by PulseAudio when it needs more audio data.
  static void StreamRequestCallback(pa_stream* s, size_t len, void* p_this);

  // Fulfill a write request from the write request callback.  Outputs silence
  // if the request could not be fulfilled.
  void FulfillWriteRequest(size_t requested_bytes);

  // Close() helper function to free internal structs.
  void Reset();

  // AudioParameters from the constructor.
  const AudioParameters params_;

  // The device ID for the device to open.
  const std::string device_id_;

  // Audio manager that created us.  Used to report that we've closed.
  AudioManagerBase* manager_;

  // PulseAudio API structs.
  pa_context* pa_context_;
  pa_threaded_mainloop* pa_mainloop_;
  pa_stream* pa_stream_;

  // Float representation of volume from 0.0 to 1.0.
  float volume_;

  // Callback to audio data source.  Must only be modified while holding a lock
  // on |pa_mainloop_| via pa_threaded_mainloop_lock().
  AudioSourceCallback* source_callback_;

  // Container for retrieving data from AudioSourceCallback::OnMoreData().
  std::unique_ptr<AudioBus> audio_bus_;

  const size_t buffer_size_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(PulseAudioOutputStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_PULSE_PULSE_OUTPUT_H_
