// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_AUDIO_VIRTUAL_AUDIO_OUTPUT_STREAM_H_
#define MEDIA_AUDIO_VIRTUAL_AUDIO_OUTPUT_STREAM_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/threading/thread_checker.h"
#include "media/audio/audio_io.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_parameters.h"

namespace media {

class VirtualAudioInputStream;

// VirtualAudioOutputStream attaches to a VirtualAudioInputStream when Start()
// is called and is used as an audio source. VirtualAudioOutputStream also
// implements an interface so it can be used as an input to AudioConverter so
// that we can get audio frames that match the AudioParameters that
// VirtualAudioInputStream expects.
class MEDIA_EXPORT VirtualAudioOutputStream
    : public AudioOutputStream,
      public AudioConverter::InputCallback {
 public:
  // Callback invoked just after VirtualAudioOutputStream is closed.
  typedef base::OnceCallback<void(VirtualAudioOutputStream* vaos)>
      AfterCloseCallback;

  // Construct an audio loopback pathway to the given |target| (not owned).
  // |target| must outlive this instance.
  VirtualAudioOutputStream(const AudioParameters& params,
                           VirtualAudioInputStream* target,
                           AfterCloseCallback after_close_cb);

  ~VirtualAudioOutputStream() override;

  // AudioOutputStream:
  bool Open() override;
  void Start(AudioSourceCallback* callback) override;
  void Stop() override;
  void SetVolume(double volume) override;
  void GetVolume(double* volume) override;
  void Close() override;
  void Flush() override;

 private:
  // AudioConverter::InputCallback:
  double ProvideInput(AudioBus* audio_bus, uint32_t frames_delayed) override;

  const AudioParameters params_;
  // Pointer to the VirtualAudioInputStream to attach to when Start() is called.
  // This pointer should always be valid because VirtualAudioInputStream should
  // outlive this class.
  VirtualAudioInputStream* const target_input_stream_;

  AfterCloseCallback after_close_cb_;

  AudioSourceCallback* callback_;
  double volume_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(VirtualAudioOutputStream);
};

}  // namespace media

#endif  // MEDIA_AUDIO_VIRTUAL_AUDIO_OUTPUT_STREAM_H_
