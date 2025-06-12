// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/webaudio/audio_context_manager_impl.h"

#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
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
    audio_context_manager_ = &AudioContextManagerImpl::CreateForTesting(
        *main_rfh(), service_remote.BindNewPipeAndPassReceiver());
    audio_context_manager_->set_clock_for_testing(&clock_);
  }

  void TearDown() override {
    audio_context_manager_ = nullptr;
    RenderViewHostTestHarness::TearDown();
  }

  AudioContextManagerImpl* audio_context_manager() {
    return audio_context_manager_;
  }

  const ukm::TestAutoSetUkmRecorder& test_ukm_recorder() const {
    return test_ukm_recorder_;
  }

  base::SimpleTestTickClock& clock() { return clock_; }
  raw_ptr<AudioContextManagerImpl> audio_context_manager_ = nullptr;

 private:
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
  base::SimpleTestTickClock clock_;
};

TEST_F(AudioContextManagerImplTest, TimeBelow10SecondsIsRaw) {
  // Entry for 42 milliseconds.
  audio_context_manager()->AudioContextAudiblePlaybackStarted(0);
  clock().Advance(base::Milliseconds(42));
  audio_context_manager()->AudioContextAudiblePlaybackStopped(0);

  // Entry for 4242 milliseconds.
  audio_context_manager()->AudioContextAudiblePlaybackStarted(0);
  clock().Advance(base::Milliseconds(4242));
  audio_context_manager()->AudioContextAudiblePlaybackStopped(0);

  // Entry for 9999 milliseconds.
  audio_context_manager()->AudioContextAudiblePlaybackStarted(0);
  clock().Advance(base::Milliseconds(9999));
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
  clock().Advance(base::Seconds(42));
  audio_context_manager()->AudioContextAudiblePlaybackStopped(0);

  // Entry for 42.42 seconds.
  audio_context_manager()->AudioContextAudiblePlaybackStarted(0);
  clock().Advance(base::Seconds(42.42));
  audio_context_manager()->AudioContextAudiblePlaybackStopped(0);

  // Entry for 10.01 seconds.
  audio_context_manager()->AudioContextAudiblePlaybackStarted(0);
  clock().Advance(base::Seconds(10.01));
  audio_context_manager()->AudioContextAudiblePlaybackStopped(0);

  // Entry for 10.99 seconds.
  audio_context_manager()->AudioContextAudiblePlaybackStarted(0);
  clock().Advance(base::Seconds(10.99));
  audio_context_manager()->AudioContextAudiblePlaybackStopped(0);

  auto ukm_entries = test_ukm_recorder().GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(4u, ukm_entries.size());

  const std::vector<int> expected = {42000, 42000, 10000, 10000};
  for (size_t i = 0; i < expected.size(); ++i) {
    EXPECT_EQ(expected[i], *test_ukm_recorder().GetEntryMetric(
                               ukm_entries[i], UkmEntry::kAudibleTimeName));
  }
}

TEST_F(AudioContextManagerImplTest, TracksMaxConcurrentContexts) {
  base::HistogramTester histogram_tester;

  // Create 2 contexts. Max should be 2.
  audio_context_manager()->AudioContextCreated(1);
  audio_context_manager()->AudioContextCreated(2);
  EXPECT_EQ(audio_context_manager()->max_concurrent_audio_contexts_, 2);

  // Close 1 context. The max count should remain at 2.
  audio_context_manager()->AudioContextClosed(1);
  EXPECT_EQ(audio_context_manager()->max_concurrent_audio_contexts_, 2);

  // Create 2 more contexts. Active contexts are now {2, 3, 4}, so the new
  // max is 3.
  audio_context_manager()->AudioContextCreated(3);
  audio_context_manager()->AudioContextCreated(4);
  EXPECT_EQ(audio_context_manager()->max_concurrent_audio_contexts_, 3);
  // Close 3 contexts, the recorded maximum concurrent count remain as 3.
  audio_context_manager()->AudioContextClosed(2);
  audio_context_manager()->AudioContextClosed(3);
  audio_context_manager()->AudioContextClosed(4);
  EXPECT_EQ(audio_context_manager()->max_concurrent_audio_contexts_, 3);

  // The UMA histogram is recorded on destruction. To test this, we must
  // trigger the destruction of the AudioContextManager by destroying its owning
  // RenderFrameHost.
  audio_context_manager_ = nullptr;
  DeleteContents();
  base::RunLoop().RunUntilIdle();

  // Verify that the final maximum value was correctly recorded.
  histogram_tester.ExpectUniqueSample(
      "WebAudio.AudioContext.ConcurrentAudioContexts",
      /*sample=*/3,
      /*expected_bucket_count=*/1);
}

TEST_F(AudioContextManagerImplTest, NoContextsCreated) {
  base::HistogramTester histogram_tester;

  // No contexts were ever created.
  EXPECT_EQ(audio_context_manager()->max_concurrent_audio_contexts_, 0);

  // Trigger destruction to record the UMA metric.
  audio_context_manager_ = nullptr;
  DeleteContents();
  base::RunLoop().RunUntilIdle();

  // Verify that the sample for 0 was correctly recorded since no context was
  // ever created.
  histogram_tester.ExpectUniqueSample(
      "WebAudio.AudioContext.ConcurrentAudioContexts",
      /*sample=*/0,
      /*expected_bucket_count=*/1);
}

}  // namespace content
