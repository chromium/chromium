// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/audible_metrics.h"

#include "base/metrics/histogram_samples.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "content/browser/media/media_web_contents_observer.h"
#include "content/public/test/test_renderer_host.h"
#include "content/test/test_web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

static const char* CONCURRENT_TAB_WHEN_STARTING_HISTOGRAM =
    "Media.Audible.ConcurrentTabsWhenStarting";
static const char* MAX_CONCURRENT_TAB_IN_SESSION_HISTOGRAM =
    "Media.Audible.MaxConcurrentTabsInSession";
static const char* CONCURRENT_TABS_TIME_HISTOGRAM =
    "Media.Audible.ConcurrentTabsTime";

class AudibleMetricsTest : public RenderViewHostTestHarness {
 public:
  AudibleMetricsTest() = default;

  AudibleMetricsTest(const AudibleMetricsTest&) = delete;
  AudibleMetricsTest& operator=(const AudibleMetricsTest&) = delete;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    audible_metrics_ = std::make_unique<AudibleMetrics>();

    // Set the clock to a value different than 0 so the time it gives is
    // recognized as initialized.
    clock_.Advance(base::Milliseconds(1));
    audible_metrics_->SetClockForTest(&clock_);
  }

  void TearDown() override {
    audible_metrics_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  base::SimpleTestTickClock* clock() { return &clock_; }

  AudibleMetrics* audible_metrics() { return audible_metrics_.get(); }

  const base::UserActionTester& user_action_tester() const {
    return user_action_tester_;
  }

  std::unique_ptr<base::HistogramSamples> GetHistogramSamplesSinceTestStart(
      const std::string& name) {
    return histogram_tester_.GetHistogramSamplesSinceCreation(name);
  }

  std::unique_ptr<TestWebContents> CreateWebContents() {
    return TestWebContents::Create(GetBrowserContext(), nullptr);
  }

  std::unique_ptr<MediaWebContentsObserver> CreateMediaObserver(
      TestWebContents* web_contents) {
    std::unique_ptr<MediaWebContentsObserver> media_observer =
        std::make_unique<MediaWebContentsObserver>(web_contents);
    media_observer->SetAudibleMetricsForTest(audible_metrics());
    return media_observer;
  }

 private:
  std::unique_ptr<AudibleMetrics> audible_metrics_;

  base::SimpleTestTickClock clock_;
  base::HistogramTester histogram_tester_;
  base::UserActionTester user_action_tester_;
};

}  // anonymous namespace

TEST_F(AudibleMetricsTest, CreateAndKillDoesNothing) {
  { std::unique_ptr<AudibleMetrics> audible_metrics(new AudibleMetrics()); }

  {
    std::unique_ptr<base::HistogramSamples> samples(
        GetHistogramSamplesSinceTestStart(
            CONCURRENT_TAB_WHEN_STARTING_HISTOGRAM));
    EXPECT_EQ(0, samples->TotalCount());
  }

  {
    std::unique_ptr<base::HistogramSamples> samples(
        GetHistogramSamplesSinceTestStart(
            MAX_CONCURRENT_TAB_IN_SESSION_HISTOGRAM));
    EXPECT_EQ(0, samples->TotalCount());
  }

  {
    std::unique_ptr<base::HistogramSamples> samples(
        GetHistogramSamplesSinceTestStart(CONCURRENT_TABS_TIME_HISTOGRAM));
    EXPECT_EQ(0, samples->TotalCount());
  }

}

TEST_F(AudibleMetricsTest, AudibleStart) {
  std::unique_ptr<TestWebContents> web_contents = CreateWebContents();

  audible_metrics()->UpdateAudibleWebContentsState(web_contents.get(), true);

  {
    std::unique_ptr<base::HistogramSamples> samples(
        GetHistogramSamplesSinceTestStart(
            CONCURRENT_TAB_WHEN_STARTING_HISTOGRAM));
    EXPECT_EQ(1, samples->TotalCount());
    EXPECT_EQ(1, samples->GetCount(0));
  }

  {
    std::unique_ptr<base::HistogramSamples> samples(
        GetHistogramSamplesSinceTestStart(
            MAX_CONCURRENT_TAB_IN_SESSION_HISTOGRAM));
    EXPECT_EQ(1, samples->TotalCount());
    EXPECT_EQ(1, samples->GetCount(1));
  }

  {
    std::unique_ptr<base::HistogramSamples> samples(
        GetHistogramSamplesSinceTestStart(CONCURRENT_TABS_TIME_HISTOGRAM));
    EXPECT_EQ(0, samples->TotalCount());
  }

}

TEST_F(AudibleMetricsTest, AudibleStartAndStop) {
  std::unique_ptr<TestWebContents> web_contents = CreateWebContents();

  audible_metrics()->UpdateAudibleWebContentsState(web_contents.get(), true);
  audible_metrics()->UpdateAudibleWebContentsState(web_contents.get(), false);

  {
    std::unique_ptr<base::HistogramSamples> samples(
        GetHistogramSamplesSinceTestStart(
            CONCURRENT_TAB_WHEN_STARTING_HISTOGRAM));
    EXPECT_EQ(1, samples->TotalCount());
    EXPECT_EQ(1, samples->GetCount(0));
  }

  {
    std::unique_ptr<base::HistogramSamples> samples(
        GetHistogramSamplesSinceTestStart(
            MAX_CONCURRENT_TAB_IN_SESSION_HISTOGRAM));
    EXPECT_EQ(1, samples->TotalCount());
    EXPECT_EQ(1, samples->GetCount(1));
  }

  {
    std::unique_ptr<base::HistogramSamples> samples(
        GetHistogramSamplesSinceTestStart(CONCURRENT_TABS_TIME_HISTOGRAM));
    EXPECT_EQ(0, samples->TotalCount());
  }
}

TEST_F(AudibleMetricsTest, AddSameTabIsNoOp) {
  std::unique_ptr<TestWebContents> web_contents = CreateWebContents();

  audible_metrics()->UpdateAudibleWebContentsState(web_contents.get(), true);
  audible_metrics()->UpdateAudibleWebContentsState(web_contents.get(), true);
  audible_metrics()->UpdateAudibleWebContentsState(web_contents.get(), true);
  audible_metrics()->UpdateAudibleWebContentsState(web_contents.get(), true);
  audible_metrics()->UpdateAudibleWebContentsState(web_contents.get(), true);

  {
    std::unique_ptr<base::HistogramSamples> samples(
        GetHistogramSamplesSinceTestStart(
            CONCURRENT_TAB_WHEN_STARTING_HISTOGRAM));
    EXPECT_EQ(1, samples->TotalCount());
    EXPECT_EQ(1, samples->GetCount(0));
  }

  {
    std::unique_ptr<base::HistogramSamples> samples(
        GetHistogramSamplesSinceTestStart(
            MAX_CONCURRENT_TAB_IN_SESSION_HISTOGRAM));
    EXPECT_EQ(1, samples->TotalCount());
    EXPECT_EQ(1, samples->GetCount(1));
  }

  {
    std::unique_ptr<base::HistogramSamples> samples(
        GetHistogramSamplesSinceTestStart(CONCURRENT_TABS_TIME_HISTOGRAM));
    EXPECT_EQ(0, samples->TotalCount());
  }
}

TEST_F(AudibleMetricsTest, RemoveUnknownTabIsNoOp) {
  std::unique_ptr<TestWebContents> web_contents = CreateWebContents();

  audible_metrics()->UpdateAudibleWebContentsState(web_contents.get(), false);
  audible_metrics()->UpdateAudibleWebContentsState(web_contents.get(), false);

  EXPECT_EQ(0, GetHistogramSamplesSinceTestStart(
      CONCURRENT_TAB_WHEN_STARTING_HISTOGRAM)->TotalCount());
  EXPECT_EQ(0, GetHistogramSamplesSinceTestStart(
      MAX_CONCURRENT_TAB_IN_SESSION_HISTOGRAM)->TotalCount());
  EXPECT_EQ(0, GetHistogramSamplesSinceTestStart(
      CONCURRENT_TABS_TIME_HISTOGRAM)->TotalCount());
}

TEST_F(AudibleMetricsTest, ConcurrentTabsInSessionIsIncremental) {
  std::unique_ptr<TestWebContents> web_contents_0 = CreateWebContents();
  std::unique_ptr<TestWebContents> web_contents_1 = CreateWebContents();
  std::unique_ptr<TestWebContents> web_contents_2 = CreateWebContents();
  std::unique_ptr<TestWebContents> web_contents_3 = CreateWebContents();

  audible_metrics()->UpdateAudibleWebContentsState(web_contents_0.get(), true);
  audible_metrics()->UpdateAudibleWebContentsState(web_contents_1.get(), true);
  audible_metrics()->UpdateAudibleWebContentsState(web_contents_2.get(), true);
  audible_metrics()->UpdateAudibleWebContentsState(web_contents_3.get(), true);

  std::unique_ptr<base::HistogramSamples> samples(
      GetHistogramSamplesSinceTestStart(
          MAX_CONCURRENT_TAB_IN_SESSION_HISTOGRAM));
  EXPECT_EQ(4, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(1));
  EXPECT_EQ(1, samples->GetCount(2));
  EXPECT_EQ(1, samples->GetCount(3));
  EXPECT_EQ(1, samples->GetCount(4));
}

TEST_F(AudibleMetricsTest, ConcurrentTabsInSessionKeepTrackOfRemovedTabs) {
  std::unique_ptr<TestWebContents> web_contents_0 = CreateWebContents();
  std::unique_ptr<TestWebContents> web_contents_1 = CreateWebContents();
  std::unique_ptr<TestWebContents> web_contents_2 = CreateWebContents();
  std::unique_ptr<TestWebContents> web_contents_3 = CreateWebContents();

  audible_metrics()->UpdateAudibleWebContentsState(web_contents_0.get(), true);
  audible_metrics()->UpdateAudibleWebContentsState(web_contents_0.get(), false);
  audible_metrics()->UpdateAudibleWebContentsState(web_contents_1.get(), true);
  audible_metrics()->UpdateAudibleWebContentsState(web_contents_2.get(), true);
  audible_metrics()->UpdateAudibleWebContentsState(web_contents_1.get(), false);
  audible_metrics()->UpdateAudibleWebContentsState(web_contents_2.get(), false);
  audible_metrics()->UpdateAudibleWebContentsState(web_contents_3.get(), true);

  std::unique_ptr<base::HistogramSamples> samples(
      GetHistogramSamplesSinceTestStart(
          MAX_CONCURRENT_TAB_IN_SESSION_HISTOGRAM));
  EXPECT_EQ(2, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(1));
  EXPECT_EQ(1, samples->GetCount(2));
}

TEST_F(AudibleMetricsTest, ConcurrentTabsInSessionIsNotCountedTwice) {
  std::unique_ptr<TestWebContents> web_contents_0 = CreateWebContents();
  std::unique_ptr<TestWebContents> web_contents_1 = CreateWebContents();
  std::unique_ptr<TestWebContents> web_contents_2 = CreateWebContents();
  std::unique_ptr<TestWebContents> web_contents_3 = CreateWebContents();

  audible_metrics()->UpdateAudibleWebContentsState(web_contents_0.get(), true);
  audible_metrics()->UpdateAudibleWebContentsState(web_contents_1.get(), true);
  audible_metrics()->UpdateAudibleWebContentsState(web_contents_2.get(), true);
  audible_metrics()->UpdateAudibleWebContentsState(web_contents_3.get(), true);

  audible_metrics()->UpdateAudibleWebContentsState(web_contents_0.get(), false);
  audible_metrics()->UpdateAudibleWebContentsState(web_contents_1.get(), false);
  audible_metrics()->UpdateAudibleWebContentsState(web_contents_2.get(), false);
  audible_metrics()->UpdateAudibleWebContentsState(web_contents_3.get(), false);

  audible_metrics()->UpdateAudibleWebContentsState(web_contents_0.get(), true);
  audible_metrics()->UpdateAudibleWebContentsState(web_contents_1.get(), true);
  audible_metrics()->UpdateAudibleWebContentsState(web_contents_2.get(), true);
  audible_metrics()->UpdateAudibleWebContentsState(web_contents_3.get(), true);

  std::unique_ptr<base::HistogramSamples> samples(
      GetHistogramSamplesSinceTestStart(
          MAX_CONCURRENT_TAB_IN_SESSION_HISTOGRAM));
  EXPECT_EQ(4, samples->TotalCount());
  EXPECT_EQ(1, samples->GetCount(1));
  EXPECT_EQ(1, samples->GetCount(2));
  EXPECT_EQ(1, samples->GetCount(3));
  EXPECT_EQ(1, samples->GetCount(4));
}

TEST_F(AudibleMetricsTest, ConcurrentTabsWhenStartingAddedPerTab) {
  std::unique_ptr<TestWebContents> web_contents_0 = CreateWebContents();
  std::unique_ptr<TestWebContents> web_contents_1 = CreateWebContents();

  audible_metrics()->UpdateAudibleWebContentsState(web_contents_0.get(), true);
  audible_metrics()->UpdateAudibleWebContentsState(web_contents_1.get(), true);

  {
    std::unique_ptr<base::HistogramSamples> samples(
        GetHistogramSamplesSinceTestStart(
            CONCURRENT_TAB_WHEN_STARTING_HISTOGRAM));
    EXPECT_EQ(2, samples->TotalCount());
    EXPECT_EQ(1, samples->GetCount(0));
    EXPECT_EQ(1, samples->GetCount(1));
  }

  // Added again: ignored.
  audible_metrics()->UpdateAudibleWebContentsState(web_contents_0.get(), true);
  audible_metrics()->UpdateAudibleWebContentsState(web_contents_1.get(), true);

  {
    std::unique_ptr<base::HistogramSamples> samples(
        GetHistogramSamplesSinceTestStart(
            CONCURRENT_TAB_WHEN_STARTING_HISTOGRAM));
    EXPECT_EQ(2, samples->TotalCount());
    EXPECT_EQ(1, samples->GetCount(0));
    EXPECT_EQ(1, samples->GetCount(1));
  }

  // Removing both.
  audible_metrics()->UpdateAudibleWebContentsState(web_contents_0.get(), false);
  audible_metrics()->UpdateAudibleWebContentsState(web_contents_1.get(), false);

  {
    std::unique_ptr<base::HistogramSamples> samples(
        GetHistogramSamplesSinceTestStart(
            CONCURRENT_TAB_WHEN_STARTING_HISTOGRAM));
    EXPECT_EQ(2, samples->TotalCount());
    EXPECT_EQ(1, samples->GetCount(0));
    EXPECT_EQ(1, samples->GetCount(1));
  }

  // Adding them after removed, it is counted.
  audible_metrics()->UpdateAudibleWebContentsState(web_contents_0.get(), true);
  audible_metrics()->UpdateAudibleWebContentsState(web_contents_1.get(), true);

  {
    std::unique_ptr<base::HistogramSamples> samples(
        GetHistogramSamplesSinceTestStart(
            CONCURRENT_TAB_WHEN_STARTING_HISTOGRAM));
    EXPECT_EQ(4, samples->TotalCount());
    EXPECT_EQ(2, samples->GetCount(0));
    EXPECT_EQ(2, samples->GetCount(1));
  }
}

TEST_F(AudibleMetricsTest, ConcurrentTabsTimeRequiresTwoAudibleTabs) {
  std::unique_ptr<TestWebContents> web_contents_0 = CreateWebContents();
  std::unique_ptr<TestWebContents> web_contents_1 = CreateWebContents();

  audible_metrics()->UpdateAudibleWebContentsState(web_contents_0.get(), true);
  audible_metrics()->UpdateAudibleWebContentsState(web_contents_1.get(), true);

  clock()->Advance(base::Milliseconds(1000));

  // No record because concurrent audible tabs still running.
  EXPECT_EQ(0, GetHistogramSamplesSinceTestStart(
      CONCURRENT_TABS_TIME_HISTOGRAM)->TotalCount());

  // No longer concurrent.
  audible_metrics()->UpdateAudibleWebContentsState(web_contents_0.get(), false);
  {
    std::unique_ptr<base::HistogramSamples> samples(
        GetHistogramSamplesSinceTestStart(CONCURRENT_TABS_TIME_HISTOGRAM));
    EXPECT_EQ(1, samples->TotalCount());
    EXPECT_EQ(1, samples->GetCount(1000));
  }

  // Stopping the second tab is a no-op.
  audible_metrics()->UpdateAudibleWebContentsState(web_contents_1.get(), false);
  {
    std::unique_ptr<base::HistogramSamples> samples(
        GetHistogramSamplesSinceTestStart(CONCURRENT_TABS_TIME_HISTOGRAM));
    EXPECT_EQ(1, samples->TotalCount());
    EXPECT_EQ(1, samples->GetCount(1000));
  }
}

TEST_F(AudibleMetricsTest, ConcurrentTabsTimeRunsAsLongAsTwoAudibleTabs) {
  std::unique_ptr<TestWebContents> web_contents_0 = CreateWebContents();
  std::unique_ptr<TestWebContents> web_contents_1 = CreateWebContents();
  std::unique_ptr<TestWebContents> web_contents_2 = CreateWebContents();

  audible_metrics()->UpdateAudibleWebContentsState(web_contents_0.get(), true);
  audible_metrics()->UpdateAudibleWebContentsState(web_contents_1.get(), true);

  clock()->Advance(base::Milliseconds(1000));

  audible_metrics()->UpdateAudibleWebContentsState(web_contents_2.get(), true);

  clock()->Advance(base::Milliseconds(500));

  // Mutes one of the three audible tabs.
  audible_metrics()->UpdateAudibleWebContentsState(web_contents_1.get(), false);

  // No record because concurrent audible tabs still running.
  EXPECT_EQ(0, GetHistogramSamplesSinceTestStart(
      CONCURRENT_TABS_TIME_HISTOGRAM)->TotalCount());

  // Mutes the first audible tab.
  audible_metrics()->UpdateAudibleWebContentsState(web_contents_0.get(), false);
  {
    std::unique_ptr<base::HistogramSamples> samples(
        GetHistogramSamplesSinceTestStart(CONCURRENT_TABS_TIME_HISTOGRAM));
    EXPECT_EQ(1, samples->TotalCount());
    EXPECT_EQ(1, samples->GetCount(1500));
  }
}

TEST_F(AudibleMetricsTest, MediaWebContentsObserver_Audible_Muted) {
  std::unique_ptr<TestWebContents> web_contents = CreateWebContents();
  std::unique_ptr<MediaWebContentsObserver> media_observer =
      CreateMediaObserver(web_contents.get());

  web_contents->SetAudioMuted(true);
  web_contents->audio_stream_monitor()->set_is_currently_audible_for_testing(
      true);

  EXPECT_TRUE(web_contents->audio_stream_monitor()->IsCurrentlyAudible());
  EXPECT_TRUE(web_contents->IsAudioMuted());

  EXPECT_EQ(0, audible_metrics()->GetAudibleWebContentsSizeForTest());
  media_observer->MaybeUpdateAudibleState();
  EXPECT_EQ(0, audible_metrics()->GetAudibleWebContentsSizeForTest());
}

TEST_F(AudibleMetricsTest, MediaWebContentsObserver_Audible_NotMuted) {
  std::unique_ptr<TestWebContents> web_contents = CreateWebContents();
  std::unique_ptr<MediaWebContentsObserver> media_observer =
      CreateMediaObserver(web_contents.get());

  web_contents->audio_stream_monitor()->set_is_currently_audible_for_testing(
      true);

  EXPECT_TRUE(web_contents->audio_stream_monitor()->IsCurrentlyAudible());
  EXPECT_FALSE(web_contents->IsAudioMuted());

  EXPECT_EQ(0, audible_metrics()->GetAudibleWebContentsSizeForTest());
  media_observer->MaybeUpdateAudibleState();
  EXPECT_EQ(1, audible_metrics()->GetAudibleWebContentsSizeForTest());
}

TEST_F(AudibleMetricsTest, MediaWebContentsObserver_NotAudible_Muted) {
  std::unique_ptr<TestWebContents> web_contents = CreateWebContents();
  std::unique_ptr<MediaWebContentsObserver> media_observer =
      CreateMediaObserver(web_contents.get());

  web_contents->SetAudioMuted(true);

  EXPECT_FALSE(web_contents->audio_stream_monitor()->IsCurrentlyAudible());
  EXPECT_TRUE(web_contents->IsAudioMuted());

  EXPECT_EQ(0, audible_metrics()->GetAudibleWebContentsSizeForTest());
  media_observer->MaybeUpdateAudibleState();
  EXPECT_EQ(0, audible_metrics()->GetAudibleWebContentsSizeForTest());
}

TEST_F(AudibleMetricsTest, MediaWebContentsObserver_NotAudible_NotMuted) {
  std::unique_ptr<TestWebContents> web_contents = CreateWebContents();
  std::unique_ptr<MediaWebContentsObserver> media_observer =
      CreateMediaObserver(web_contents.get());

  EXPECT_FALSE(web_contents->audio_stream_monitor()->IsCurrentlyAudible());
  EXPECT_FALSE(web_contents->IsAudioMuted());

  EXPECT_EQ(0, audible_metrics()->GetAudibleWebContentsSizeForTest());
  media_observer->MaybeUpdateAudibleState();
  EXPECT_EQ(0, audible_metrics()->GetAudibleWebContentsSizeForTest());
}

}  // namespace content
