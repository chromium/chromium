// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <tuple>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/test/render_view_test.h"
#include "content/renderer/media/renderer_webmediaplayer_delegate.h"
#include "content/renderer/render_process.h"
#include "media/base/media_content_type.h"
#include "media/base/media_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/platform/scheduler/test/renderer_scheduler_test_support.h"

using testing::NiceMock;
using testing::Return;
using testing::StrictMock;

namespace media {

namespace {
constexpr base::TimeDelta kIdleTimeout = base::Seconds(1);
}

class MockWebMediaPlayerDelegateObserver
    : public blink::WebMediaPlayerDelegate::Observer {
 public:
  MockWebMediaPlayerDelegateObserver() {}
  ~MockWebMediaPlayerDelegateObserver() {}

  // WebMediaPlayerDelegate::Observer implementation.
  MOCK_METHOD0(OnFrameHidden, void());
  MOCK_METHOD0(OnFrameShown, void());
  MOCK_METHOD0(OnIdleTimeout, void());
};

class RendererWebMediaPlayerDelegateTest : public content::RenderViewTest {
 public:
  RendererWebMediaPlayerDelegateTest() {}

  RendererWebMediaPlayerDelegateTest(
      const RendererWebMediaPlayerDelegateTest&) = delete;
  RendererWebMediaPlayerDelegateTest& operator=(
      const RendererWebMediaPlayerDelegateTest&) = delete;

  ~RendererWebMediaPlayerDelegateTest() override {}

  void SetUp() override {
    RenderViewTest::SetUp();
    // Start the tick clock off at a non-null value.
    tick_clock_.Advance(base::Seconds(1234));
    delegate_manager_ =
        std::make_unique<RendererWebMediaPlayerDelegate>(GetMainRenderFrame());
    delegate_manager_->SetIdleCleanupParamsForTesting(
        kIdleTimeout, base::TimeDelta(), &tick_clock_, false);
  }

  void TearDown() override {
    delegate_manager_.reset();
    RenderViewTest::TearDown();
  }

 protected:
  IPC::TestSink& test_sink() { return render_thread_->sink(); }

  void SetIsLowEndDeviceForTesting() {
    delegate_manager_->SetIdleCleanupParamsForTesting(
        kIdleTimeout, base::TimeDelta(), &tick_clock_, true);
  }

  void SetNonZeroIdleTimeout() {
    delegate_manager_->SetIdleCleanupParamsForTesting(
        kIdleTimeout, base::Seconds(1), &tick_clock_, true);
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
};

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
  delegate_manager_->DidMediaMetadataChange(
      delegate_id_1, true, true, media::MediaContentType::Persistent);
  delegate_manager_->DidPlay(delegate_id_1);
  delegate_manager_->SetIdle(delegate_id_1, false);
  tick_clock_.Advance(base::Microseconds(1));
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
  tick_clock_.Advance(base::Microseconds(1));
  RunLoopOnce();
}

TEST_F(RendererWebMediaPlayerDelegateTest,
       SuspendRequestsAreOnlySentOnceIfHandled) {
  int delegate_id_1 = delegate_manager_->AddObserver(&observer_1_);
  delegate_manager_->SetIdle(delegate_id_1, true);
  EXPECT_CALL(observer_1_, OnIdleTimeout());
  tick_clock_.Advance(kIdleTimeout + base::Microseconds(1));
  base::RunLoop().RunUntilIdle();
}

TEST_F(RendererWebMediaPlayerDelegateTest,
       SuspendRequestsAreOnlySentOnceIfNotHandled) {
  SetNonZeroIdleTimeout();
  int delegate_id_1 = delegate_manager_->AddObserver(&observer_1_);
  delegate_manager_->SetIdle(delegate_id_1, true);
  EXPECT_CALL(observer_1_, OnIdleTimeout());
  tick_clock_.Advance(kIdleTimeout + base::Microseconds(1));
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
    tick_clock_.Advance(kIdleTimeout + base::Microseconds(1));
    RunLoopOnce();
  }

  // Once the player is idle, it should be suspended after |kIdleTimeout|.
  delegate_manager_->SetIdle(delegate_id_1, true);
  {
    EXPECT_CALL(observer_1_, OnIdleTimeout());
    tick_clock_.Advance(kIdleTimeout + base::Microseconds(1));
    RunLoopOnce();
  }
}

}  // namespace media
