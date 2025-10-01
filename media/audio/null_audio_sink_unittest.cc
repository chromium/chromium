// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/null_audio_sink.h"

#include "base/test/task_environment.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_parameters.h"
#include "media/base/audio_renderer_sink.h"
#include "media/base/test_helpers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class NullAudioSinkTest : public testing::Test,
                          public AudioRendererSink::RenderCallback {
 public:
  NullAudioSinkTest() = default;

  NullAudioSinkTest(const NullAudioSinkTest&) = delete;
  NullAudioSinkTest& operator=(const NullAudioSinkTest&) = delete;

  ~NullAudioSinkTest() override = default;

  scoped_refptr<NullAudioSink> ConstructSink() {
    auto new_sink = base::MakeRefCounted<NullAudioSink>(
        task_environment_.GetMainThreadTaskRunner());
    return new_sink;
  }

  AudioParameters CreateAudioParameters() {
    return AudioParameters(AudioParameters::AUDIO_PCM_LOW_LATENCY,
                           ChannelLayoutConfig::Stereo(), 48000, 1024);
  }

  // AudioRendererSink::RenderCallback implementation.
  int Render(base::TimeDelta delay,
             base::TimeTicks delay_timestamp,
             const AudioGlitchInfo& glitch_info,
             AudioBus* dest) override {
    // Wake up WaitForPendingRender() on Render() call.
    if (wait_for_pending_render_cb_) {
      std::move(wait_for_pending_render_cb_).Run();
    }
    return dest->frames();
  }
  MOCK_METHOD0(OnRenderError, void());

  void WaitForPendingRender() {
    WaitableMessageLoopEvent event;
    wait_for_pending_render_cb_ = event.GetClosure();
    event.RunAndWait();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::OnceClosure wait_for_pending_render_cb_;
};

TEST_F(NullAudioSinkTest, PlayAfterStop) {
  scoped_refptr<NullAudioSink> sink = ConstructSink();
  AudioParameters params = CreateAudioParameters();

  // Setup the sink for playing then ensure Stop() is called.
  sink->Initialize(params, this);
  sink->Start();
  sink->Play();
  sink->Stop();

  // Now resume playback.
  sink->Start();
  sink->Play();

  // Test that the Render() callback occurs
  WaitForPendingRender();

  sink->Stop();

  // Allow any pending tasks to complete before test ends
  task_environment_.RunUntilIdle();
}

}  // namespace media
