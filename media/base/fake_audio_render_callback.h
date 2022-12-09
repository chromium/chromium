// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_BASE_FAKE_AUDIO_RENDER_CALLBACK_H_
#define MEDIA_BASE_FAKE_AUDIO_RENDER_CALLBACK_H_

#include <stdint.h>

#include "base/time/time.h"
#include "media/base/audio_converter.h"
#include "media/base/audio_renderer_sink.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace media {

// Fake RenderCallback which will fill each request with a sine wave.  Sine
// state is kept across callbacks.  State can be reset to default via reset().
// Also provide an interface to AudioTransformInput.
class FakeAudioRenderCallback : public AudioRendererSink::RenderCallback,
                                public AudioConverter::InputCallback {
 public:
  // The function used to fulfill Render() is f(x) = sin(2 * PI * x * |step|),
  // where x = [|number_of_frames| * m, |number_of_frames| * (m + 1)] and m =
  // the number of Render() calls fulfilled thus far.
  FakeAudioRenderCallback(double step, int sample_rate);

  FakeAudioRenderCallback(const FakeAudioRenderCallback&) = delete;
  FakeAudioRenderCallback& operator=(const FakeAudioRenderCallback&) = delete;

  ~FakeAudioRenderCallback() override;

  // Renders a sine wave into the provided audio data buffer.  If |half_fill_|
  // is set, will only fill half the buffer.
  int Render(base::TimeDelta delay,
             base::TimeTicks delay_timestamp,
             const AudioGlitchInfo& glitch_info,
             AudioBus* audio_bus) override;
  MOCK_METHOD0(OnRenderError, void());

  // AudioConverter::InputCallback implementation.
  double ProvideInput(AudioBus* audio_bus,
                      uint32_t frames_delayed,
                      const AudioGlitchInfo& glitch_info) override;

  // Toggles only filling half the requested amount during Render().
  void set_half_fill(bool half_fill) { half_fill_ = half_fill; }

  // Reset the sine state to initial value.
  void reset() { x_ = 0; }

  // Returns the last |delay| provided to Render() or base::TimeDelta::Max()
  // if no Render() call occurred.
  base::TimeDelta last_delay() const { return last_delay_; }

  // Set volume information used by ProvideInput().
  void set_volume(double volume) { volume_ = volume; }

  void set_needs_fade_in(bool needs_fade_in) { needs_fade_in_ = needs_fade_in; }

  int last_channel_count() const { return last_channel_count_; }

  media::AudioGlitchInfo cumulative_glitch_info() const {
    return cumulative_glitch_info_;
  }

 private:
  int RenderInternal(AudioBus* audio_bus, base::TimeDelta delay, double volume);

  bool half_fill_;
  double x_;
  double step_;
  base::TimeDelta last_delay_;
  int last_channel_count_;
  double volume_;
  int sample_rate_;
  bool needs_fade_in_ = false;
  media::AudioGlitchInfo cumulative_glitch_info_;
};

}  // namespace media

#endif  // MEDIA_BASE_FAKE_AUDIO_RENDER_CALLBACK_H_
