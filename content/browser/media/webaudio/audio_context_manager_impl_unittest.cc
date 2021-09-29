// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/webaudio/audio_context_manager_impl.h"

#include <vector>

#include "base/test/simple_test_tick_clock.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/test_renderer_host.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace content {

class AudioContextManagerImplTest : public RenderViewHostTestHarness {
 public:
  using UkmEntry = ukm::builders::Media_WebAudio_AudioContext_AudibleTime;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    clock_.SetNowTicks(base::TimeTicks::Now());

    mojo::Remote<blink::mojom::AudioContextManager> service_remote;
    audio_context_manager_ = new AudioContextManagerImpl(
        main_rfh(), service_remote.BindNewPipeAndPassReceiver());
    audio_context_manager_->set_clock_for_testing(&clock_);
  }

  AudioContextManagerImpl* audio_context_manager() {
    return audio_context_manager_;
  }

  const ukm::TestAutoSetUkmRecorder& test_ukm_recorder() const {
    return test_ukm_recorder_;
  }

  base::SimpleTestTickClock& clock() { return clock_; }

 private:
  AudioContextManagerImpl* audio_context_manager_ = nullptr;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
  base::SimpleTestTickClock clock_;
};

TEST_F(AudioContextManagerImplTest, TimeBelow10SecondsIsRaw) {
  // Entry for 42 milliseconds.
  audio_context_manager()->AudioContextAudiblePlaybackStarted(0);
  clock().Advance(base::TimeDelta::FromMilliseconds(42));
  audio_context_manager()->AudioContextAudiblePlaybackStopped(0);

  // Entry for 4242 milliseconds.
  audio_context_manager()->AudioContextAudiblePlaybackStarted(0);
  clock().Advance(base::TimeDelta::FromMilliseconds(4242));
  audio_context_manager()->AudioContextAudiblePlaybackStopped(0);

  // Entry for 9999 milliseconds.
  audio_context_manager()->AudioContextAudiblePlaybackStarted(0);
  clock().Advance(base::TimeDelta::FromMilliseconds(9999));
  audio_context_manager()->AudioContextAudiblePlaybackStopped(0);

  auto ukm_entries = test_ukm_recorder().GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(3u, ukm_entries.size());

  const std::vector<int> expected = {42, 4242, 9999};
  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(expected[i], *test_ukm_recorder().GetEntryMetric(
                               ukm_entries[i], UkmEntry::kAudibleTimeName));
  }
}

TEST_F(AudioContextManagerImplTest, TimeGreater10SecondsIsRoundedDown) {
  // Entry for 42 seconds.
  audio_context_manager()->AudioContextAudiblePlaybackStarted(0);
  clock().Advance(base::TimeDelta::FromSeconds(42));
  audio_context_manager()->AudioContextAudiblePlaybackStopped(0);

  // Entry for 42.42 seconds.
  audio_context_manager()->AudioContextAudiblePlaybackStarted(0);
  clock().Advance(base::TimeDelta::FromSecondsD(42.42));
  audio_context_manager()->AudioContextAudiblePlaybackStopped(0);

  // Entry for 10.01 seconds.
  audio_context_manager()->AudioContextAudiblePlaybackStarted(0);
  clock().Advance(base::TimeDelta::FromSecondsD(10.01));
  audio_context_manager()->AudioContextAudiblePlaybackStopped(0);

  // Entry for 10.99 seconds.
  audio_context_manager()->AudioContextAudiblePlaybackStarted(0);
  clock().Advance(base::TimeDelta::FromSecondsD(10.99));
  audio_context_manager()->AudioContextAudiblePlaybackStopped(0);

  auto ukm_entries = test_ukm_recorder().GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(4u, ukm_entries.size());

  const std::vector<int> expected = {42000, 42000, 10000, 10000};
  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(expected[i], *test_ukm_recorder().GetEntryMetric(
                               ukm_entries[i], UkmEntry::kAudibleTimeName));
  }
}

}  // namespace content
