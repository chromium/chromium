// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/webrtc_overrides/metronome_source.h"
#include <memory>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/api/task_queue/task_queue_base.h"
#include "third_party/webrtc_overrides/timer_based_tick_provider.h"

namespace blink {
namespace {

using ::testing::InSequence;
using ::testing::Invoke;
using ::testing::Mock;
using ::testing::MockFunction;
using ::testing::Return;

class MockTickProvider : public MetronomeSource::TickProvider {
 public:
  MOCK_METHOD(void, RequestCallOnNextTick, (base::OnceClosure), (override));

  // Estimate the current tick period.
  MOCK_METHOD(base::TimeDelta, TickPeriod, (), (override));
};

class MetronomeSourceTest : public ::testing::Test {
 public:
  MetronomeSourceTest() {
    auto tick_provider = base::MakeRefCounted<MockTickProvider>();
    tick_provider_ptr_ = tick_provider.get();
    source_ = std::make_unique<MetronomeSource>(std::move(tick_provider));
  }

 protected:
  std::unique_ptr<MetronomeSource> source_;
  raw_ptr<MockTickProvider> tick_provider_ptr_;
};

TEST_F(MetronomeSourceTest, SupportsCallsBeyondSourceLifetime) {
  auto metronome = source_->CreateWebRtcMetronome();

  metronome->RequestCallOnNextTick([] {});
  tick_provider_ptr_ = nullptr;
  source_ = nullptr;

  // This just makes use of the metronome after the source is gone.
  metronome->RequestCallOnNextTick([] {});
  metronome->TickPeriod();
}

TEST_F(MetronomeSourceTest, InvokesRequestedCallbackOnTick) {
  auto metronome = source_->CreateWebRtcMetronome();
  MockFunction<void()> callback;

  // Provision a fake tick function.
  base::OnceClosure do_tick;
  EXPECT_CALL(*tick_provider_ptr_, RequestCallOnNextTick)
      .WillOnce(Invoke(
          [&](base::OnceClosure closure) { do_tick = std::move(closure); }));
  metronome->RequestCallOnNextTick(callback.AsStdFunction());

  EXPECT_CALL(callback, Call);
  std::move(do_tick).Run();
}

TEST_F(MetronomeSourceTest, InvokesTwoCallbacksOnSameTick) {
  auto metronome = source_->CreateWebRtcMetronome();
  MockFunction<void()> callback;

  // Provision a fake tick function.
  base::OnceClosure do_tick;
  EXPECT_CALL(*tick_provider_ptr_, RequestCallOnNextTick)
      .WillOnce(Invoke(
          [&](base::OnceClosure closure) { do_tick = std::move(closure); }));
  metronome->RequestCallOnNextTick(callback.AsStdFunction());
  metronome->RequestCallOnNextTick(callback.AsStdFunction());

  EXPECT_CALL(callback, Call).Times(2);
  std::move(do_tick).Run();
}

TEST_F(MetronomeSourceTest,
       InvokesRequestedCallbackOnNewTickFromCallbackOnTick) {
  auto metronome = source_->CreateWebRtcMetronome();
  MockFunction<void()> callback1;
  MockFunction<void()> callback2;
  base::OnceClosure do_tick1;
  base::OnceClosure do_tick2;
  InSequence s;
  EXPECT_CALL(*tick_provider_ptr_, RequestCallOnNextTick)
      .WillRepeatedly(Invoke(
          [&](base::OnceClosure closure) { do_tick1 = std::move(closure); }));
  EXPECT_CALL(callback1, Call).WillOnce(Invoke([&] {
    metronome->RequestCallOnNextTick(callback2.AsStdFunction());
  }));
  EXPECT_CALL(*tick_provider_ptr_, RequestCallOnNextTick)
      .WillRepeatedly(Invoke(
          [&](base::OnceClosure closure) { do_tick2 = std::move(closure); }));
  EXPECT_CALL(callback2, Call);
  metronome->RequestCallOnNextTick(callback1.AsStdFunction());
  std::move(do_tick1).Run();
  std::move(do_tick2).Run();
}

TEST_F(MetronomeSourceTest, ReturnsTickProviderTickPeriod) {
  constexpr base::TimeDelta kTickPeriod = base::Seconds(4711);
  EXPECT_CALL(*tick_provider_ptr_, TickPeriod).WillOnce(Return(kTickPeriod));
  EXPECT_EQ(kTickPeriod.InMicroseconds(),
            source_->CreateWebRtcMetronome()->TickPeriod().us());
}

}  // namespace
}  // namespace blink
