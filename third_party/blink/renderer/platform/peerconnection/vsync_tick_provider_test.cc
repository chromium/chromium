// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/peerconnection/vsync_tick_provider.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "metronome_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/peerconnection/vsync_provider.h"

namespace blink {
namespace {

using ::testing::Mock;

class FakeVSyncProvider : public VSyncProvider {
 public:
  void RunVSyncCallback() {
    if (vsync_callback_) {
      std::move(vsync_callback_).Run();
      vsync_callback_.Reset();
    }
  }

  void SetTabVisible(bool visible) { tab_visible_callback_.Run(visible); }

  // VSyncProvider overrides.
  void Initialize(
      base::RepeatingCallback<void(bool /*visible*/)> callback) override {
    tab_visible_callback_ = std::move(callback);
  }
  void SetVSyncCallback(base::OnceClosure callback) override {
    vsync_callback_ = std::move(callback);
  }

 private:
  base::OnceClosure vsync_callback_;
  base::RepeatingCallback<void(bool /*visible*/)> tab_visible_callback_;
};

class FakeDefaultTickProvider : public MetronomeSource::TickProvider {
 public:
  static constexpr base::TimeDelta kTickPeriod = base::Microseconds(4711);

  // MetronomeSource::TickProvider overrides.
  void RequestCallOnNextTick(base::OnceClosure callback) override {
    callbacks_.push_back(std::move(callback));
  }
  base::TimeDelta TickPeriod() override { return kTickPeriod; }

  void RunCallbacks() {
    for (auto&& callback : callbacks_)
      std::move(callback).Run();
    callbacks_.clear();
  }

 private:
  std::vector<base::OnceClosure> callbacks_;
};

class VSyncTickProviderTest : public ::testing::Test {
 public:
  VSyncTickProviderTest() {
    fake_default_tick_provider_ =
        base::MakeRefCounted<FakeDefaultTickProvider>();
    begin_frame_tick_provider_ = VSyncTickProvider::Create(
        fake_begin_frame_provider_,
        base::SequencedTaskRunner::GetCurrentDefault(),
        fake_default_tick_provider_);
    DepleteTaskQueues();
  }

  void DepleteTaskQueues() {
    task_environment_.FastForwardBy(base::Seconds(0));
  }

  void SetTabVisible(bool visible) {
    fake_begin_frame_provider_.SetTabVisible(visible);
    DepleteTaskQueues();
  }

  void RunVSyncCallback() {
    fake_begin_frame_provider_.RunVSyncCallback();
    DepleteTaskQueues();
  }

  void RunDefaultCallbacks() {
    fake_default_tick_provider_->RunCallbacks();
    DepleteTaskQueues();
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeVSyncProvider fake_begin_frame_provider_;
  scoped_refptr<VSyncTickProvider> begin_frame_tick_provider_;
  scoped_refptr<FakeDefaultTickProvider> fake_default_tick_provider_;
};

TEST_F(VSyncTickProviderTest, ReportsDefaultTickPeriod) {
  EXPECT_EQ(begin_frame_tick_provider_->TickPeriod(),
            FakeDefaultTickProvider::kTickPeriod);
}

TEST_F(VSyncTickProviderTest, ReportsDefaultTickPeriodDuringTransition) {
  // Begin switching over to be driven by vsyncs and expect the
  // TickPeriod() is the default tick period before a vsync is received.
  SetTabVisible(true);
  EXPECT_EQ(begin_frame_tick_provider_->TickPeriod(),
            FakeDefaultTickProvider::kTickPeriod);
}

TEST_F(VSyncTickProviderTest, ReportsVSyncTickPeriod) {
  // Switch over to be driven by vsyncs and expect the TickPeriod()
  // is the vsync period.
  SetTabVisible(true);
  RunVSyncCallback();
  EXPECT_EQ(begin_frame_tick_provider_->TickPeriod(),
            VSyncTickProvider::kVSyncTickPeriod);
}

TEST_F(VSyncTickProviderTest, ReportsDefaultTickPeriodAfterSwitchBack) {
  // Switch back to default provider from vsync mode and expect the
  // TickPeriod() is the default tick period.
  SetTabVisible(true);
  RunVSyncCallback();
  SetTabVisible(false);
  EXPECT_EQ(begin_frame_tick_provider_->TickPeriod(),
            FakeDefaultTickProvider::kTickPeriod);
}

TEST_F(VSyncTickProviderTest, DispatchesDefaultTicks) {
  base::MockOnceClosure closure;
  begin_frame_tick_provider_->RequestCallOnNextTick(closure.Get());
  EXPECT_CALL(closure, Run);
  RunDefaultCallbacks();
  Mock::VerifyAndClearExpectations(&closure);
  base::MockOnceClosure closure2;
  begin_frame_tick_provider_->RequestCallOnNextTick(closure2.Get());
  EXPECT_CALL(closure2, Run);
  RunDefaultCallbacks();
}

TEST_F(VSyncTickProviderTest, DispatchesDefaultTicksDuringSwitch) {
  SetTabVisible(true);

  base::MockOnceClosure closure;
  begin_frame_tick_provider_->RequestCallOnNextTick(closure.Get());
  EXPECT_CALL(closure, Run);
  RunDefaultCallbacks();

  base::MockOnceClosure closure2;
  begin_frame_tick_provider_->RequestCallOnNextTick(closure2.Get());
  EXPECT_CALL(closure2, Run);
  RunDefaultCallbacks();
}

TEST_F(VSyncTickProviderTest, DispatchesCallbackOnSwitch) {
  base::MockOnceClosure closure;
  begin_frame_tick_provider_->RequestCallOnNextTick(closure.Get());
  SetTabVisible(true);
  EXPECT_CALL(closure, Run);
  RunVSyncCallback();

  // Since we are now in vsync mode, old default callbacks should not be
  // dispatching.
  base::MockOnceClosure closure2;
  EXPECT_CALL(closure2, Run).Times(0);
  begin_frame_tick_provider_->RequestCallOnNextTick(closure2.Get());
  RunDefaultCallbacks();
}

TEST_F(VSyncTickProviderTest, DispatchesVSyncs) {
  SetTabVisible(true);
  RunVSyncCallback();

  base::MockOnceClosure closure;
  begin_frame_tick_provider_->RequestCallOnNextTick(closure.Get());
  EXPECT_CALL(closure, Run);
  RunVSyncCallback();

  base::MockOnceClosure closure2;
  begin_frame_tick_provider_->RequestCallOnNextTick(closure2.Get());
  EXPECT_CALL(closure2, Run);
  RunVSyncCallback();
}

TEST_F(VSyncTickProviderTest, DispatchesDefaultAfterSwitchBackFromVSyncs) {
  SetTabVisible(true);
  RunVSyncCallback();
  SetTabVisible(false);

  base::MockOnceClosure closure;
  begin_frame_tick_provider_->RequestCallOnNextTick(closure.Get());
  EXPECT_CALL(closure, Run);
  RunDefaultCallbacks();

  // Old vsync callbacks must not dispatch out.
  base::MockOnceClosure closure2;
  begin_frame_tick_provider_->RequestCallOnNextTick(closure2.Get());
  EXPECT_CALL(closure2, Run).Times(0);
  RunVSyncCallback();
}

TEST_F(VSyncTickProviderTest,
       DispatchesCallbackRequestedBeforeSwitchBackFromVSyncs) {
  // Request a callback during vsyncs, and switch back. The callback
  // should be invoked after the switch.
  SetTabVisible(true);
  RunVSyncCallback();
  base::MockOnceClosure closure;
  begin_frame_tick_provider_->RequestCallOnNextTick(closure.Get());
  SetTabVisible(false);

  EXPECT_CALL(closure, Run);
  RunDefaultCallbacks();
}

TEST_F(VSyncTickProviderTest, IgnoresVSyncsAfterDefaultSwitchback) {
  // Switch to vsync mode.
  SetTabVisible(true);
  RunVSyncCallback();

  // Register a callback, and then switch back to default mode before
  // it's dispatched.
  base::MockOnceClosure closure;
  begin_frame_tick_provider_->RequestCallOnNextTick(closure.Get());
  SetTabVisible(false);

  // After this, vsyncs should not dispatch the registered callback.
  EXPECT_CALL(closure, Run).Times(0);
  RunVSyncCallback();
}

TEST_F(VSyncTickProviderTest, MultipleMetronomeAreAlignedOnTick) {
  std::unique_ptr<MetronomeSource> source1 =
      std::make_unique<MetronomeSource>(begin_frame_tick_provider_);
  std::unique_ptr<MetronomeSource> source2 =
      std::make_unique<MetronomeSource>(begin_frame_tick_provider_);
  auto metronome1 = source1->CreateWebRtcMetronome();
  auto metronome2 = source2->CreateWebRtcMetronome();

  testing::MockFunction<void()> callback1;
  testing::MockFunction<void()> callback2;
  metronome1->RequestCallOnNextTick(callback1.AsStdFunction());
  metronome2->RequestCallOnNextTick(callback2.AsStdFunction());

  // Default tick used to align the metronomes.
  EXPECT_CALL(callback1, Call());
  EXPECT_CALL(callback2, Call());
  RunDefaultCallbacks();

  SetTabVisible(true);
  RunVSyncCallback();

  metronome1->RequestCallOnNextTick(callback1.AsStdFunction());
  metronome2->RequestCallOnNextTick(callback2.AsStdFunction());

  // VSync tick used to align the metronomes when tab visible.
  EXPECT_CALL(callback1, Call());
  EXPECT_CALL(callback2, Call());
  RunVSyncCallback();
}

}  // namespace
}  // namespace blink
