// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <tuple>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/common/media/media_player_delegate_messages.h"
#include "content/public/renderer/render_view.h"
#include "content/public/test/render_view_test.h"
#include "content/renderer/media/renderer_webmediaplayer_delegate.h"
#include "content/renderer/render_process.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"

using testing::NiceMock;
using testing::Return;
using testing::StrictMock;

namespace media {

namespace {
constexpr base::TimeDelta kIdleTimeout = base::TimeDelta::FromSeconds(1);
}

class MockWebMediaPlayerDelegateObserver
    : public blink::WebMediaPlayerDelegate::Observer {
 public:
  MockWebMediaPlayerDelegateObserver() {}
  ~MockWebMediaPlayerDelegateObserver() {}

  // WebMediaPlayerDelegate::Observer implementation.
  MOCK_METHOD0(OnFrameHidden, void());
  MOCK_METHOD0(OnFrameClosed, void());
  MOCK_METHOD0(OnFrameShown, void());
  MOCK_METHOD0(OnIdleTimeout, void());
  MOCK_METHOD0(OnPlay, void());
  MOCK_METHOD0(OnPause, void());
  MOCK_METHOD1(OnMuted, void(bool));
  MOCK_METHOD1(OnSeekForward, void(double));
  MOCK_METHOD1(OnSeekBackward, void(double));
  MOCK_METHOD1(OnVolumeMultiplierUpdate, void(double));
  MOCK_METHOD1(OnBecamePersistentVideo, void(bool));
  MOCK_METHOD0(OnPictureInPictureModeEnded, void());
};

class RendererWebMediaPlayerDelegateTest : public content::RenderViewTest {
 public:
  RendererWebMediaPlayerDelegateTest() {}
  ~RendererWebMediaPlayerDelegateTest() override {}

  void SetUp() override {
    RenderViewTest::SetUp();
    // Start the tick clock off at a non-null value.
    tick_clock_.Advance(base::TimeDelta::FromSeconds(1234));
    delegate_manager_.reset(
        new RendererWebMediaPlayerDelegate(view_->GetMainRenderFrame()));
    delegate_manager_->SetIdleCleanupParamsForTesting(
        kIdleTimeout, base::TimeDelta(), &tick_clock_, false);
  }

  void TearDown() override {
    delegate_manager_.reset();
    RenderViewTest::TearDown();
  }

 protected:
  IPC::TestSink& test_sink() { return render_thread_->sink(); }

  void CallOnMediaDelegatePlay(int delegate_id) {
    delegate_manager_->OnMediaDelegatePlay(delegate_id);
  }

  void CallOnMediaDelegatePause(int delegate_id) {
    delegate_manager_->OnMediaDelegatePause(delegate_id,
                                            true /* triggered_by_user */);
  }

  void SetIsLowEndDeviceForTesting() {
    delegate_manager_->SetIdleCleanupParamsForTesting(
        kIdleTimeout, base::TimeDelta(), &tick_clock_, true);
  }

  void SetNonZeroIdleTimeout() {
    delegate_manager_->SetIdleCleanupParamsForTesting(
        kIdleTimeout, base::TimeDelta::FromSeconds(1), &tick_clock_, true);
  }

  void RunLoopOnce() {
    base::RunLoop run_loop;
    blink::scheduler::GetSingleThreadTaskRunnerForTesting()->PostTask(
        FROM_HERE, run_loop.QuitClosure());
    run_loop.Run();
  }

  std::unique_ptr<RendererWebMediaPlayerDelegate> delegate_manager_;
  StrictMock<MockWebMediaPlayerDelegateObserver> observer_1_, observer_2_,
      observer_3_;
  base::SimpleTestTickClock tick_clock_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RendererWebMediaPlayerDelegateTest);
};

TEST_F(RendererWebMediaPlayerDelegateTest, SendsMessagesCorrectly) {
  StrictMock<MockWebMediaPlayerDelegateObserver> observer;
  const int delegate_id = delegate_manager_->AddObserver(&observer);

  // Verify the playing message.
  {
    const bool kHasVideo = true, kHasAudio = false, kIsRemote = false;
    const media::MediaContentType kMediaContentType =
        media::MediaContentType::Transient;
    delegate_manager_->DidPlay(delegate_id, kHasVideo, kHasAudio,
                               kMediaContentType);

    const IPC::Message* msg = test_sink().GetUniqueMessageMatching(
        MediaPlayerDelegateHostMsg_OnMediaPlaying::ID);
    ASSERT_TRUE(msg);

    std::tuple<int, bool, bool, bool, media::MediaContentType> result;
    ASSERT_TRUE(MediaPlayerDelegateHostMsg_OnMediaPlaying::Read(msg, &result));
    EXPECT_EQ(delegate_id, std::get<0>(result));
    EXPECT_EQ(kHasVideo, std::get<1>(result));
    EXPECT_EQ(kHasAudio, std::get<2>(result));
    EXPECT_EQ(kIsRemote, std::get<3>(result));
    EXPECT_EQ(kMediaContentType, std::get<4>(result));
  }

  // Verify the paused message.
  {
    test_sink().ClearMessages();
    const bool kReachedEndOfStream = false;
    delegate_manager_->DidPause(delegate_id);

    const IPC::Message* msg = test_sink().GetUniqueMessageMatching(
        MediaPlayerDelegateHostMsg_OnMediaPaused::ID);
    ASSERT_TRUE(msg);

    std::tuple<int, bool> result;
    ASSERT_TRUE(MediaPlayerDelegateHostMsg_OnMediaPaused::Read(msg, &result));
    EXPECT_EQ(delegate_id, std::get<0>(result));
    EXPECT_EQ(kReachedEndOfStream, std::get<1>(result));
  }

  // Verify the destruction message.
  {
    test_sink().ClearMessages();
    delegate_manager_->PlayerGone(delegate_id);
    const IPC::Message* msg = test_sink().GetUniqueMessageMatching(
        MediaPlayerDelegateHostMsg_OnMediaDestroyed::ID);
    ASSERT_TRUE(msg);

    std::tuple<int> result;
    ASSERT_TRUE(
        MediaPlayerDelegateHostMsg_OnMediaDestroyed::Read(msg, &result));
    EXPECT_EQ(delegate_id, std::get<0>(result));
  }

  // Verify the resize message.
  {
    test_sink().ClearMessages();
    delegate_manager_->DidPlayerSizeChange(delegate_id, gfx::Size(16, 9));
    const IPC::Message* msg = test_sink().GetUniqueMessageMatching(
        MediaPlayerDelegateHostMsg_OnMediaSizeChanged::ID);
    ASSERT_TRUE(msg);

    std::tuple<int, gfx::Size> result;
    ASSERT_TRUE(
        MediaPlayerDelegateHostMsg_OnMediaSizeChanged::Read(msg, &result));
    EXPECT_EQ(delegate_id, std::get<0>(result));
    EXPECT_EQ(16, std::get<1>(result).width());
    EXPECT_EQ(9, std::get<1>(result).height());
  }

  // Verify the muted status message.
  {
    test_sink().ClearMessages();
    delegate_manager_->DidPlayerMutedStatusChange(delegate_id, true);
    const IPC::Message* msg = test_sink().GetUniqueMessageMatching(
        MediaPlayerDelegateHostMsg_OnMutedStatusChanged::ID);
    ASSERT_TRUE(msg);

    std::tuple<int, bool> result;
    ASSERT_TRUE(
        MediaPlayerDelegateHostMsg_OnMutedStatusChanged::Read(msg, &result));
    EXPECT_EQ(delegate_id, std::get<0>(result));
    EXPECT_TRUE(std::get<1>(result));
  }
}

TEST_F(RendererWebMediaPlayerDelegateTest, DeliversObserverNotifications) {
  const int delegate_id = delegate_manager_->AddObserver(&observer_1_);

  EXPECT_CALL(observer_1_, OnFrameHidden());
  delegate_manager_->WasHidden();

  EXPECT_CALL(observer_1_, OnFrameShown());
  delegate_manager_->WasShown();

  EXPECT_CALL(observer_1_, OnPause());
  MediaPlayerDelegateMsg_Pause pause_msg(0, delegate_id,
                                         true /* triggered_by_user */);
  delegate_manager_->OnMessageReceived(pause_msg);

  EXPECT_CALL(observer_1_, OnPlay());
  MediaPlayerDelegateMsg_Play play_msg(0, delegate_id);
  delegate_manager_->OnMessageReceived(play_msg);

  const double kTestSeekForwardSeconds = 1.0;
  EXPECT_CALL(observer_1_, OnSeekForward(kTestSeekForwardSeconds));
  MediaPlayerDelegateMsg_SeekForward seek_forward_msg(
      0, delegate_id, base::TimeDelta::FromSeconds(kTestSeekForwardSeconds));
  delegate_manager_->OnMessageReceived(seek_forward_msg);

  const double kTestSeekBackwardSeconds = 2.0;
  EXPECT_CALL(observer_1_, OnSeekBackward(kTestSeekBackwardSeconds));
  MediaPlayerDelegateMsg_SeekBackward seek_backward_msg(
      0, delegate_id, base::TimeDelta::FromSeconds(kTestSeekBackwardSeconds));
  delegate_manager_->OnMessageReceived(seek_backward_msg);

  const double kTestMultiplier = 0.5;
  EXPECT_CALL(observer_1_, OnVolumeMultiplierUpdate(kTestMultiplier));
  MediaPlayerDelegateMsg_UpdateVolumeMultiplier volume_msg(0, delegate_id,
                                                           kTestMultiplier);
  delegate_manager_->OnMessageReceived(volume_msg);

  EXPECT_CALL(observer_1_, OnFrameClosed());
  MediaPlayerDelegateMsg_SuspendAllMediaPlayers suspend_msg(0);
  delegate_manager_->OnMessageReceived(suspend_msg);
}

TEST_F(RendererWebMediaPlayerDelegateTest, TheTimerIsInitiallyStopped) {
  ASSERT_FALSE(delegate_manager_->IsIdleCleanupTimerRunningForTesting());
}

TEST_F(RendererWebMediaPlayerDelegateTest, AddingAnIdleObserverStartsTheTimer) {
  const int delegate_id_1 = delegate_manager_->AddObserver(&observer_1_);
  delegate_manager_->SetIdle(delegate_id_1, true);
  RunLoopOnce();
  ASSERT_TRUE(delegate_manager_->IsIdleCleanupTimerRunningForTesting());
}

TEST_F(RendererWebMediaPlayerDelegateTest, RemovingAllObserversStopsTheTimer) {
  const int delegate_id_1 = delegate_manager_->AddObserver(&observer_1_);
  delegate_manager_->SetIdle(delegate_id_1, true);
  RunLoopOnce();
  delegate_manager_->RemoveObserver(delegate_id_1);
  RunLoopOnce();
  ASSERT_FALSE(delegate_manager_->IsIdleCleanupTimerRunningForTesting());
}

TEST_F(RendererWebMediaPlayerDelegateTest, PlaySuspendsLowEndIdleDelegates) {
  SetIsLowEndDeviceForTesting();

  const int delegate_id_1 = delegate_manager_->AddObserver(&observer_1_);
  delegate_manager_->SetIdle(delegate_id_1, true);
  const int delegate_id_2 = delegate_manager_->AddObserver(&observer_2_);
  delegate_manager_->SetIdle(delegate_id_2, true);
  RunLoopOnce();

  // Calling play on the first player should suspend the other idle player.
  EXPECT_CALL(observer_2_, OnIdleTimeout());
  delegate_manager_->DidPlay(delegate_id_1, true, true,
                             media::MediaContentType::Persistent);
  delegate_manager_->SetIdle(delegate_id_1, false);
  tick_clock_.Advance(base::TimeDelta::FromMicroseconds(1));
  RunLoopOnce();
}

TEST_F(RendererWebMediaPlayerDelegateTest, MaxLowEndIdleDelegates) {
  SetIsLowEndDeviceForTesting();

  int delegate_id_1 = delegate_manager_->AddObserver(&observer_1_);
  delegate_manager_->SetIdle(delegate_id_1, true);
  int delegate_id_2 = delegate_manager_->AddObserver(&observer_2_);
  delegate_manager_->SetIdle(delegate_id_2, true);
  RunLoopOnce();

  // Just adding a third idle observer should suspend all idle players.
  EXPECT_CALL(observer_1_, OnIdleTimeout());
  EXPECT_CALL(observer_2_, OnIdleTimeout());
  int delegate_id_3 = delegate_manager_->AddObserver(&observer_3_);
  delegate_manager_->SetIdle(delegate_id_3, true);
  EXPECT_CALL(observer_3_, OnIdleTimeout());
  tick_clock_.Advance(base::TimeDelta::FromMicroseconds(1));
  RunLoopOnce();
}

TEST_F(RendererWebMediaPlayerDelegateTest,
       SuspendRequestsAreOnlySentOnceIfHandled) {
  int delegate_id_1 = delegate_manager_->AddObserver(&observer_1_);
  delegate_manager_->SetIdle(delegate_id_1, true);
  EXPECT_CALL(observer_1_, OnIdleTimeout());
  tick_clock_.Advance(kIdleTimeout + base::TimeDelta::FromMicroseconds(1));
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererWebMediaPlayerDelegateTest,
       SuspendRequestsAreOnlySentOnceIfNotHandled) {
  SetNonZeroIdleTimeout();
  int delegate_id_1 = delegate_manager_->AddObserver(&observer_1_);
  delegate_manager_->SetIdle(delegate_id_1, true);
  EXPECT_CALL(observer_1_, OnIdleTimeout());
  tick_clock_.Advance(kIdleTimeout + base::TimeDelta::FromMicroseconds(1));
  base::RunLoop().RunUntilIdle();
  delegate_manager_->ClearStaleFlag(delegate_id_1);
  ASSERT_TRUE(delegate_manager_->IsIdleCleanupTimerRunningForTesting());
  // Make sure that OnIdleTimeout isn't called again immediately.
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererWebMediaPlayerDelegateTest, IdleDelegatesAreSuspended) {
  // Add one non-idle observer and one idle observer.
  const int delegate_id_1 = delegate_manager_->AddObserver(&observer_1_);
  const int delegate_id_2 = delegate_manager_->AddObserver(&observer_2_);
  delegate_manager_->SetIdle(delegate_id_2, true);

  // The idle cleanup task should suspend the second delegate while the first is
  // kept alive.
  {
    EXPECT_CALL(observer_2_, OnIdleTimeout());
    tick_clock_.Advance(kIdleTimeout + base::TimeDelta::FromMicroseconds(1));
    RunLoopOnce();
  }

  // Once the player is idle, it should be suspended after |kIdleTimeout|.
  delegate_manager_->SetIdle(delegate_id_1, true);
  {
    EXPECT_CALL(observer_1_, OnIdleTimeout());
    tick_clock_.Advance(kIdleTimeout + base::TimeDelta::FromMicroseconds(1));
    RunLoopOnce();
  }
}

#if defined(OS_ANDROID)

TEST_F(RendererWebMediaPlayerDelegateTest, Histograms) {
  NiceMock<MockWebMediaPlayerDelegateObserver> observer;
  int delegate_id = delegate_manager_->AddObserver(&observer);
  base::HistogramTester histogram_tester;
  histogram_tester.ExpectTotalCount("Media.Android.BackgroundVideoTime", 0);

  // Play/pause while not hidden doesn't record anything.
  delegate_manager_->DidPlay(delegate_id, true, true,
                             MediaContentType::Persistent);
  RunLoopOnce();
  delegate_manager_->DidPause(delegate_id);
  RunLoopOnce();
  histogram_tester.ExpectTotalCount("Media.Android.BackgroundVideoTime", 0);

  // Play/pause while hidden does.
  delegate_manager_->SetFrameHiddenForTesting(true);
  delegate_manager_->DidPlay(delegate_id, true, true,
                             MediaContentType::Persistent);
  RunLoopOnce();
  delegate_manager_->DidPause(delegate_id);
  RunLoopOnce();
  histogram_tester.ExpectTotalCount("Media.Android.BackgroundVideoTime", 1);

  // As does ending background playback by becoming visible.
  delegate_manager_->SetFrameHiddenForTesting(true);
  delegate_manager_->DidPlay(delegate_id, true, true,
                             MediaContentType::Persistent);
  RunLoopOnce();
  delegate_manager_->SetFrameHiddenForTesting(false);
  RunLoopOnce();
  histogram_tester.ExpectTotalCount("Media.Android.BackgroundVideoTime", 2);
}

#endif  // OS_ANDROID

}  // namespace media
