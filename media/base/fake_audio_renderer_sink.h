// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_FAKE_AUDIO_RENDERER_SINK_H_
#define MEDIA_BASE_FAKE_AUDIO_RENDERER_SINK_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/output_device_info.h"

namespace media {

class FakeAudioRendererSink : public AudioRendererSink {
 public:
  enum State {
    kUninitialized,
    kInitialized,
    kStarted,
    kPaused,
    kPlaying,
    kStopped
  };

  FakeAudioRendererSink();

  explicit FakeAudioRendererSink(const AudioParameters& hardware_params);

  void Initialize(const AudioParameters& params,
                  RenderCallback* callback) override;
  void Start() override;
  void Stop() override;
  void Flush() override;
  void Pause() override;
  void Play() override;
  bool SetVolume(double volume) override;
  OutputDeviceInfo GetOutputDeviceInfo() override;
  void GetOutputDeviceInfoAsync(OutputDeviceInfoCB info_cb) override;
  bool IsOptimizedForHardwareParameters() override;
  bool CurrentThreadIsRenderingThread() override;

  // Attempts to call Render() on the callback provided to
  // Initialize() with |dest| and |delay|.
  // Returns true and sets |frames_written| to the return value of the
  // Render() call.
  // Returns false if this object is in a state where calling Render()
  // should not occur. (i.e., in the kPaused or kStopped state.) The
  // value of |frames_written| is undefined if false is returned.
  bool Render(AudioBus* dest, base::TimeDelta delay, int* frames_written);
  void OnRenderError();

  // Enables different tests to have different settings.
  void SetIsOptimizedForHardwareParameters(bool value);

  State state() const { return state_; }

 protected:
  ~FakeAudioRendererSink() override;

 private:
  void ChangeState(State new_state);

  State state_;
  RenderCallback* callback_;
  OutputDeviceInfo output_device_info_;
  bool is_optimized_for_hw_params_;

  DISALLOW_COPY_AND_ASSIGN(FakeAudioRendererSink);
};

}  // namespace media

#endif  // MEDIA_BASE_FAKE_AUDIO_RENDERER_SINK_H_
