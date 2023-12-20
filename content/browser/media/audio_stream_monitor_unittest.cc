// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/audio_stream_monitor.h"

#include <map>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/browser/invalidate_type.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "media/base/audio_power_monitor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_test_helper.h"
#endif

using ::testing::InvokeWithoutArgs;

namespace content {

namespace {

const GlobalRenderFrameHostId kRenderFrameHostId = {1, 2};
const GlobalRenderFrameHostId kRenderFrameHostIdSameProcess = {1, 3};
const GlobalRenderFrameHostId kRenderFrameHostIdOtherProcess = {4, 5};
const int kStreamId = 6;
const int kAnotherStreamId = 7;

// Used to confirm audio indicator state changes occur at the correct times.
class MockWebContentsDelegate : public WebContentsDelegate {
 public:
  MOCK_METHOD2(NavigationStateChanged,
               void(WebContents* source, InvalidateTypes changed_flags));
};

}  // namespace

class AudioStreamMonitorTest : public RenderViewHostTestHarness {
 public:
  AudioStreamMonitorTest()
      : RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {
    // Start time at non-zero.
    task_environment()->FastForwardBy(base::Seconds(1000000));
  }

  AudioStreamMonitorTest(const AudioStreamMonitorTest&) = delete;
  AudioStreamMonitorTest& operator=(const AudioStreamMonitorTest&) = delete;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();

    WebContentsImpl* web_contents = static_cast<WebContentsImpl*>(
        RenderViewHostTestHarness::web_contents());
    web_contents->SetDelegate(&mock_web_contents_delegate_);

    monitor_ = web_contents->audio_stream_monitor();
  }

  void TearDown() override {
    monitor_ = nullptr;
    RenderViewHostTestHarness::TearDown();
  }

  void FastForwardBy(const base::TimeDelta& delta) {
    task_environment()->FastForwardBy(delta);
  }

  void ExpectIsMonitoring(GlobalRenderFrameHostId render_frame_host_id,
                          int stream_id,
                          bool is_polling) {
    const AudioStreamMonitor::StreamID key = {render_frame_host_id, stream_id};
    EXPECT_EQ(is_polling,
              monitor_->streams_.find(key) != monitor_->streams_.end());
  }

  void ExpectTabWasRecentlyAudible(
      bool was_audible,
      const base::TimeTicks& last_became_silent_time) {
    EXPECT_EQ(was_audible, monitor_->indicator_is_on_);
    EXPECT_EQ(last_became_silent_time, monitor_->last_became_silent_time_);
    EXPECT_EQ(monitor_->off_timer_.IsRunning(),
              monitor_->indicator_is_on_ && !monitor_->IsCurrentlyAudible() &&
                  base::TimeTicks::Now() <
                      monitor_->last_became_silent_time_ + holding_period());
  }

  void ExpectIsCurrentlyAudible() const {
    EXPECT_TRUE(monitor_->IsCurrentlyAudible());
  }

  void ExpectNotCurrentlyAudible() const {
    EXPECT_FALSE(monitor_->IsCurrentlyAudible());
  }

  void ExpectRecentlyAudibleChangeNotification(bool new_recently_audible) {
    EXPECT_CALL(
        mock_web_contents_delegate_,
        NavigationStateChanged(RenderViewHostTestHarness::web_contents(),
                               INVALIDATE_TYPE_AUDIO))
        .WillOnce(InvokeWithoutArgs(
            this, new_recently_audible
                      ? &AudioStreamMonitorTest::ExpectWasRecentlyAudible
                      : &AudioStreamMonitorTest::ExpectNotRecentlyAudible))
        .RetiresOnSaturation();
  }

  void ExpectCurrentlyAudibleChangeNotification(bool new_audible) {
    EXPECT_CALL(
        mock_web_contents_delegate_,
        NavigationStateChanged(RenderViewHostTestHarness::web_contents(),
                               INVALIDATE_TYPE_AUDIO))
        .WillOnce(InvokeWithoutArgs(
            this, new_audible
                      ? &AudioStreamMonitorTest::ExpectIsCurrentlyAudible
                      : &AudioStreamMonitorTest::ExpectNotCurrentlyAudible))
        .RetiresOnSaturation();
  }

  // A small time step useful for testing the passage of time.
  static base::TimeDelta one_time_step() { return base::Seconds(1) / 15; }

  static base::TimeDelta holding_period() {
    return base::Milliseconds(AudioStreamMonitor::kHoldOnMilliseconds);
  }

  void StartMonitoring(GlobalRenderFrameHostId render_frame_host_id,
                       int stream_id) {
    monitor_->StartMonitoringStreamOnUIThread(
        AudioStreamMonitor::StreamID{render_frame_host_id, stream_id});
  }

  void StopMonitoring(GlobalRenderFrameHostId render_frame_host_id,
                      int stream_id) {
    monitor_->StopMonitoringStreamOnUIThread(
        AudioStreamMonitor::StreamID{render_frame_host_id, stream_id});
  }

  void UpdateAudibleState(GlobalRenderFrameHostId render_frame_host_id,
                          int stream_id,
                          bool is_audible) {
    monitor_->UpdateStreamAudibleStateOnUIThread(
        AudioStreamMonitor::StreamID{render_frame_host_id, stream_id},
        is_audible);
  }

  WebContents* web_contents() { return monitor_->web_contents_; }

 protected:
  raw_ptr<AudioStreamMonitor> monitor_;

 private:
  void ExpectWasRecentlyAudible() const {
    EXPECT_TRUE(monitor_->WasRecentlyAudible());
  }

  void ExpectNotRecentlyAudible() const {
    EXPECT_FALSE(monitor_->WasRecentlyAudible());
  }

#if BUILDFLAG(IS_CHROMEOS_LACROS)
  // Instantiate LacrosService for WakeLock support.
  chromeos::ScopedLacrosServiceTestHelper scoped_lacros_service_test_helper_;
#endif

  MockWebContentsDelegate mock_web_contents_delegate_;
};

TEST_F(AudioStreamMonitorTest, MonitorsWhenProvidedAStream) {
  EXPECT_FALSE(monitor_->WasRecentlyAudible());
  ExpectNotCurrentlyAudible();
  ExpectIsMonitoring(kRenderFrameHostId, kStreamId, false);

  StartMonitoring(kRenderFrameHostId, kStreamId);
  EXPECT_FALSE(monitor_->WasRecentlyAudible());
  ExpectNotCurrentlyAudible();
  ExpectIsMonitoring(kRenderFrameHostId, kStreamId, true);

  StopMonitoring(kRenderFrameHostId, kStreamId);
  EXPECT_FALSE(monitor_->WasRecentlyAudible());
  ExpectNotCurrentlyAudible();
  ExpectIsMonitoring(kRenderFrameHostId, kStreamId, false);
}

// Tests that AudioStreamMonitor keeps the indicator on for the holding period
// even if there is silence during the holding period.
// See comments in audio_stream_monitor.h for expected behavior.
TEST_F(AudioStreamMonitorTest, IndicatorIsOnUntilHoldingPeriodHasPassed) {
  StartMonitoring(kRenderFrameHostId, kStreamId);

  // Expect WebContents will get one call form AudioStreamMonitor to toggle the
  // indicator upon the very first notification that the stream has become
  // audible.
  ExpectRecentlyAudibleChangeNotification(true);

  // Loop, each time testing a slightly longer period of silence.  The recently
  // audible state should not change while the currently audible one should.
  int num_silence_steps = 1;
  base::TimeTicks last_became_silent_time;
  do {
    ExpectCurrentlyAudibleChangeNotification(true);

    UpdateAudibleState(kRenderFrameHostId, kStreamId, true);
    ExpectTabWasRecentlyAudible(true, last_became_silent_time);
    FastForwardBy(one_time_step());

    ExpectCurrentlyAudibleChangeNotification(false);

    // Notify that the stream has become silent and advance time repeatedly,
    // ensuring that the indicator is being held on during the holding period.
    UpdateAudibleState(kRenderFrameHostId, kStreamId, false);
    last_became_silent_time = base::TimeTicks::Now();
    ExpectTabWasRecentlyAudible(true, last_became_silent_time);
    for (int i = 0; i < num_silence_steps; ++i) {
      // If the next time step will cause the holding period to expire, then a
      // notification will be sent.
      if (base::TimeTicks::Now() + one_time_step() >=
          last_became_silent_time + holding_period()) {
        ExpectRecentlyAudibleChangeNotification(false);
      }

      ExpectTabWasRecentlyAudible(true, last_became_silent_time);
      FastForwardBy(one_time_step());
    }

    ++num_silence_steps;
  } while (base::TimeTicks::Now() < last_became_silent_time + holding_period());

  // At this point, the time has just advanced to beyond the holding period and
  // the tab indicator has been turned off. Also, make sure it stays off for
  // several cycles thereafter.
  for (int i = 0; i < 10; ++i) {
    ExpectTabWasRecentlyAudible(false, last_became_silent_time);
    FastForwardBy(one_time_step());
  }
}

// Tests that the AudioStreamMonitor correctly processes updates from two
// different streams in the same tab.
TEST_F(AudioStreamMonitorTest, HandlesMultipleStreamUpdate) {
  StartMonitoring(kRenderFrameHostId, kStreamId);
  StartMonitoring(kRenderFrameHostIdSameProcess, kAnotherStreamId);

  base::TimeTicks last_became_silent_time;
  ExpectTabWasRecentlyAudible(false, last_became_silent_time);
  ExpectNotCurrentlyAudible();

  // The first stream becomes audible and the second stream is silent. The tab
  // becomes audible.
  ExpectRecentlyAudibleChangeNotification(true);
  ExpectCurrentlyAudibleChangeNotification(true);

  UpdateAudibleState(kRenderFrameHostId, kStreamId, true);
  UpdateAudibleState(kRenderFrameHostIdSameProcess, kAnotherStreamId, false);
  ExpectTabWasRecentlyAudible(true, last_became_silent_time);
  ExpectIsCurrentlyAudible();

  // Halfway through the holding period, the second stream joins in.  The
  // indicator stays on.
  FastForwardBy(holding_period() / 2);
  UpdateAudibleState(kRenderFrameHostIdSameProcess, kAnotherStreamId, true);
  ExpectTabWasRecentlyAudible(true, last_became_silent_time);
  ExpectIsCurrentlyAudible();

  // Now, both streams become silent. The tab becoms silent but the indicator
  // stays on.
  ExpectCurrentlyAudibleChangeNotification(false);
  UpdateAudibleState(kRenderFrameHostId, kStreamId, false);
  UpdateAudibleState(kRenderFrameHostIdSameProcess, kAnotherStreamId, false);
  last_became_silent_time = base::TimeTicks::Now();
  ExpectNotCurrentlyAudible();
  ExpectTabWasRecentlyAudible(true, last_became_silent_time);

  // Advance half a holding period and the indicator should still be on.
  FastForwardBy(holding_period() / 2);
  ExpectTabWasRecentlyAudible(true, last_became_silent_time);
  ExpectNotCurrentlyAudible();

  // The first stream becomes audible again during the holding period.
  // The tab becomes audible and the indicator stays on.
  ExpectCurrentlyAudibleChangeNotification(true);
  UpdateAudibleState(kRenderFrameHostId, kStreamId, true);
  ExpectTabWasRecentlyAudible(true, last_became_silent_time);
  ExpectIsCurrentlyAudible();

  // Advance a holding period. The original holding period has expired but the
  // indicator should stay on because a stream became audible in the meantime.
  FastForwardBy(holding_period() / 2);
  ExpectTabWasRecentlyAudible(true, last_became_silent_time);
  ExpectIsCurrentlyAudible();

  // The first stream becomes silent again. The tab becomes silent and the
  // indicator is still on.
  ExpectCurrentlyAudibleChangeNotification(false);
  UpdateAudibleState(kRenderFrameHostId, kStreamId, false);
  last_became_silent_time = base::TimeTicks::Now();
  ExpectTabWasRecentlyAudible(true, last_became_silent_time);
  ExpectNotCurrentlyAudible();

  // After a holding period passes, the indicator turns off.
  ExpectRecentlyAudibleChangeNotification(false);
  FastForwardBy(holding_period());
  ExpectTabWasRecentlyAudible(false, last_became_silent_time);
  ExpectNotCurrentlyAudible();

  // Now, the second stream becomes audible and the first one remains silent.
  // The tab becomes audible again.
  ExpectRecentlyAudibleChangeNotification(true);
  ExpectCurrentlyAudibleChangeNotification(true);
  UpdateAudibleState(kRenderFrameHostIdSameProcess, kAnotherStreamId, true);
  ExpectTabWasRecentlyAudible(true, last_became_silent_time);
  ExpectIsCurrentlyAudible();

  // From here onwards, both streams are silent.  Halfway through the holding
  // period, the tab is no longer audible but stays as recently audible.
  ExpectCurrentlyAudibleChangeNotification(false);
  UpdateAudibleState(kRenderFrameHostIdSameProcess, kAnotherStreamId, false);
  last_became_silent_time = base::TimeTicks::Now();
  FastForwardBy(holding_period() / 2);
  ExpectTabWasRecentlyAudible(true, last_became_silent_time);
  ExpectNotCurrentlyAudible();

  // Just past the holding period, the tab is no longer marked as recently
  // audible.
  ExpectRecentlyAudibleChangeNotification(false);
  FastForwardBy(holding_period() -
                (base::TimeTicks::Now() - last_became_silent_time));
  ExpectTabWasRecentlyAudible(false, last_became_silent_time);
  ExpectNotCurrentlyAudible();

  // The passage of time should not turn the indicator back while both streams
  // are remaining silent.
  for (int i = 0; i < 100; ++i) {
    FastForwardBy(one_time_step());
    ExpectTabWasRecentlyAudible(false, last_became_silent_time);
    ExpectNotCurrentlyAudible();
  }
}

TEST_F(AudioStreamMonitorTest, MultipleRendererProcesses) {
  StartMonitoring(kRenderFrameHostId, kStreamId);
  StartMonitoring(kRenderFrameHostIdOtherProcess, kStreamId);
  ExpectIsMonitoring(kRenderFrameHostId, kStreamId, true);
  ExpectIsMonitoring(kRenderFrameHostIdOtherProcess, kStreamId, true);
  StopMonitoring(kRenderFrameHostIdOtherProcess, kStreamId);
  ExpectIsMonitoring(kRenderFrameHostId, kStreamId, true);
  ExpectIsMonitoring(kRenderFrameHostIdOtherProcess, kStreamId, false);
}

TEST_F(AudioStreamMonitorTest, RenderProcessGone) {
  StartMonitoring(kRenderFrameHostId, kStreamId);
  StartMonitoring(kRenderFrameHostIdOtherProcess, kStreamId);
  ExpectIsMonitoring(kRenderFrameHostId, kStreamId, true);
  ExpectIsMonitoring(kRenderFrameHostIdOtherProcess, kStreamId, true);
  monitor_->RenderProcessGone(kRenderFrameHostId.child_id);
  ExpectIsMonitoring(kRenderFrameHostId, kStreamId, false);
  monitor_->RenderProcessGone(kRenderFrameHostIdOtherProcess.child_id);
  ExpectIsMonitoring(kRenderFrameHostIdOtherProcess, kStreamId, false);
}

TEST_F(AudioStreamMonitorTest, RenderFrameGone) {
  RenderFrameHost* render_frame_host = web_contents()->GetPrimaryMainFrame();
  GlobalRenderFrameHostId render_frame_host_id =
      render_frame_host->GetGlobalId();

  StartMonitoring(render_frame_host_id, kStreamId);
  StartMonitoring(kRenderFrameHostIdOtherProcess, kStreamId);
  ExpectIsMonitoring(render_frame_host_id, kStreamId, true);
  ExpectIsMonitoring(kRenderFrameHostIdOtherProcess, kStreamId, true);

  monitor_->RenderFrameDeleted(render_frame_host);
  ExpectIsMonitoring(render_frame_host_id, kStreamId, false);
  ExpectIsMonitoring(kRenderFrameHostIdOtherProcess, kStreamId, true);
}

TEST_F(AudioStreamMonitorTest, OneAudibleClient) {
  ExpectNotCurrentlyAudible();

  auto* render_frame_host_impl =
      static_cast<RenderFrameHostImpl*>(web_contents()->GetPrimaryMainFrame());
  GlobalRenderFrameHostId host_id = render_frame_host_impl->GetGlobalId();

  ExpectRecentlyAudibleChangeNotification(true);
  ExpectCurrentlyAudibleChangeNotification(true);
  auto registration = monitor_->RegisterAudibleClient(host_id);
  ExpectIsCurrentlyAudible();

  ExpectCurrentlyAudibleChangeNotification(false);
  registration.reset();
  ExpectNotCurrentlyAudible();
}

TEST_F(AudioStreamMonitorTest, MultipleAudibleClients) {
  ExpectNotCurrentlyAudible();

  auto* render_frame_host_impl =
      static_cast<RenderFrameHostImpl*>(web_contents()->GetPrimaryMainFrame());
  GlobalRenderFrameHostId host_id = render_frame_host_impl->GetGlobalId();

  // Add one client and the tab becomes audible.
  ExpectRecentlyAudibleChangeNotification(true);
  ExpectCurrentlyAudibleChangeNotification(true);
  auto registration1 = monitor_->RegisterAudibleClient(host_id);
  ExpectIsCurrentlyAudible();

  // Add another client and the tab remains audible.
  auto registration2 = monitor_->RegisterAudibleClient(host_id);
  ExpectIsCurrentlyAudible();

  // Removes one client and the tab remains audible.
  registration1.reset();
  ExpectIsCurrentlyAudible();

  // Removes another client and the tab is not audible.
  ExpectCurrentlyAudibleChangeNotification(false);
  registration2.reset();
  ExpectNotCurrentlyAudible();
}

TEST_F(AudioStreamMonitorTest, MultipleAudibleClientsMultipleRenderFrames) {
  ExpectNotCurrentlyAudible();
  // We need to navigate once before we can add child frames.
  const char kDefaultTestUrl[] = "https://google.com/";
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL(kDefaultTestUrl));

  auto* render_frame_host_tester =
      RenderFrameHostTester::For(web_contents()->GetPrimaryMainFrame());

  auto* render_frame_host_impl_1 = static_cast<RenderFrameHostImpl*>(
      render_frame_host_tester->AppendChild("child_1"));
  auto* render_frame_host_impl_2 = static_cast<RenderFrameHostImpl*>(
      render_frame_host_tester->AppendChild("child_2"));

  GlobalRenderFrameHostId host_id1 = render_frame_host_impl_1->GetGlobalId();
  GlobalRenderFrameHostId host_id2 = render_frame_host_impl_2->GetGlobalId();

  // Add one client and the tab becomes audible.
  ExpectRecentlyAudibleChangeNotification(true);
  ExpectCurrentlyAudibleChangeNotification(true);
  auto registration1 = monitor_->RegisterAudibleClient(host_id1);
  ExpectIsCurrentlyAudible();

  // Add another client and the tab remains audible.
  auto registration2 = monitor_->RegisterAudibleClient(host_id2);
  ExpectIsCurrentlyAudible();

  // Removes one client and the tab remains audible.
  registration1.reset();
  ExpectIsCurrentlyAudible();

  // Removes another client and the tab is not audible.
  ExpectCurrentlyAudibleChangeNotification(false);
  registration2.reset();
  ExpectNotCurrentlyAudible();
}

TEST_F(AudioStreamMonitorTest, AudibleClientAndStream) {
  StartMonitoring(kRenderFrameHostId, kStreamId);
  ExpectNotCurrentlyAudible();

  auto* render_frame_host_impl =
      static_cast<RenderFrameHostImpl*>(web_contents()->GetPrimaryMainFrame());
  GlobalRenderFrameHostId host_id = render_frame_host_impl->GetGlobalId();

  // Add one client and the tab becomes audible.
  ExpectRecentlyAudibleChangeNotification(true);
  ExpectCurrentlyAudibleChangeNotification(true);
  auto registration = monitor_->RegisterAudibleClient(host_id);
  ExpectIsCurrentlyAudible();

  // The stream becomes audible and the tab remains audible.
  UpdateAudibleState(kRenderFrameHostId, kStreamId, true);
  ExpectIsCurrentlyAudible();

  // Remove the client and the tab remains audible.
  registration.reset();
  ExpectIsCurrentlyAudible();

  // The stream becomes not audible and the tab is not audible.
  ExpectCurrentlyAudibleChangeNotification(false);
  UpdateAudibleState(kRenderFrameHostId, kStreamId, false);
  ExpectNotCurrentlyAudible();
}

}  // namespace content
