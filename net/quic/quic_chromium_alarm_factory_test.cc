// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_chromium_alarm_factory.h"

#include "net/quic/test_task_runner.h"
#include "net/third_party/quiche/src/quiche/quic/test_tools/mock_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::test {
namespace {

class TestDelegate : public quic::QuicAlarm::DelegateWithoutContext {
 public:
  TestDelegate() = default;

  void OnAlarm() override { fired_ = true; }

  bool fired() const { return fired_; }
  void Clear() { fired_ = false; }

 private:
  bool fired_ = false;
};

class QuicChromiumAlarmFactoryTest : public ::testing::Test {
 protected:
  QuicChromiumAlarmFactoryTest()
      : runner_(base::MakeRefCounted<TestTaskRunner>(&clock_)),
        alarm_factory_(runner_.get(), &clock_) {}

  scoped_refptr<TestTaskRunner> runner_;
  QuicChromiumAlarmFactory alarm_factory_;
  quic::MockClock clock_;
};

TEST_F(QuicChromiumAlarmFactoryTest, CreateAlarm) {
  TestDelegate* delegate = new TestDelegate();
  std::unique_ptr<quic::QuicAlarm> alarm(alarm_factory_.CreateAlarm(delegate));

  // Set the deadline 1µs in the future.
  constexpr quic::QuicTime::Delta kDelta =
      quic::QuicTime::Delta::FromMicroseconds(1);
  quic::QuicTime deadline = clock_.Now() + kDelta;
  alarm->Set(deadline);
  EXPECT_TRUE(alarm->IsSet());
  EXPECT_EQ(alarm->deadline(), deadline);
  EXPECT_FALSE(delegate->fired());

  runner_->FastForwardBy(kDelta);

  EXPECT_EQ(quic::QuicTime::Zero() + kDelta, clock_.Now());
  EXPECT_FALSE(alarm->IsSet());
  EXPECT_TRUE(delegate->fired());
}

TEST_F(QuicChromiumAlarmFactoryTest, CreateAlarmAndCancel) {
  TestDelegate* delegate = new TestDelegate();
  std::unique_ptr<quic::QuicAlarm> alarm(alarm_factory_.CreateAlarm(delegate));

  constexpr quic::QuicTime::Delta kDelta =
      quic::QuicTime::Delta::FromMicroseconds(1);
  quic::QuicTime deadline = clock_.Now() + kDelta;
  alarm->Set(deadline);
  EXPECT_TRUE(alarm->IsSet());
  EXPECT_EQ(alarm->deadline(), deadline);
  EXPECT_FALSE(delegate->fired());

  alarm->Cancel();

  EXPECT_FALSE(alarm->IsSet());
  EXPECT_FALSE(delegate->fired());

  // Advancing time should not cause the alarm to fire.
  runner_->FastForwardBy(kDelta);

  EXPECT_EQ(quic::QuicTime::Zero() + kDelta, clock_.Now());
  EXPECT_FALSE(alarm->IsSet());
  EXPECT_FALSE(delegate->fired());
}

TEST_F(QuicChromiumAlarmFactoryTest, CreateAlarmAndReset) {
  TestDelegate* delegate = new TestDelegate();
  std::unique_ptr<quic::QuicAlarm> alarm(alarm_factory_.CreateAlarm(delegate));

  // Set the deadline 1µs in the future.
  constexpr quic::QuicTime::Delta kDelta =
      quic::QuicTime::Delta::FromMicroseconds(1);
  quic::QuicTime deadline = clock_.Now() + kDelta;
  alarm->Set(deadline);
  EXPECT_TRUE(alarm->IsSet());
  EXPECT_EQ(alarm->deadline(), deadline);
  EXPECT_FALSE(delegate->fired());

  alarm->Cancel();

  EXPECT_FALSE(alarm->IsSet());
  EXPECT_FALSE(delegate->fired());

  // Set the timer with a longer delta.
  constexpr quic::QuicTime::Delta kNewDelta =
      quic::QuicTime::Delta::FromMicroseconds(3);
  quic::QuicTime new_deadline = clock_.Now() + kNewDelta;
  alarm->Set(new_deadline);
  EXPECT_TRUE(alarm->IsSet());
  EXPECT_EQ(alarm->deadline(), new_deadline);
  EXPECT_FALSE(delegate->fired());

  // Advancing time for the first delay should not cause the alarm to fire.
  runner_->FastForwardBy(kDelta);

  EXPECT_EQ(quic::QuicTime::Zero() + kDelta, clock_.Now());
  EXPECT_TRUE(alarm->IsSet());
  EXPECT_FALSE(delegate->fired());

  // Advancing time for the remaining of the new delay will fire the alarm.
  runner_->FastForwardBy(kNewDelta - kDelta);

  EXPECT_EQ(quic::QuicTime::Zero() + kNewDelta, clock_.Now());
  EXPECT_FALSE(alarm->IsSet());
  EXPECT_TRUE(delegate->fired());
}

TEST_F(QuicChromiumAlarmFactoryTest, CreateAlarmAndResetEarlier) {
  TestDelegate* delegate = new TestDelegate();
  std::unique_ptr<quic::QuicAlarm> alarm(alarm_factory_.CreateAlarm(delegate));

  // Set the deadline 3µs in the future.
  constexpr quic::QuicTime::Delta kDelta =
      quic::QuicTime::Delta::FromMicroseconds(3);
  quic::QuicTime deadline = clock_.Now() + kDelta;
  alarm->Set(deadline);
  EXPECT_TRUE(alarm->IsSet());
  EXPECT_EQ(alarm->deadline(), deadline);
  EXPECT_FALSE(delegate->fired());

  alarm->Cancel();

  EXPECT_FALSE(alarm->IsSet());
  EXPECT_FALSE(delegate->fired());

  // Set the timer with a shorter delta.
  constexpr quic::QuicTime::Delta kNewDelta =
      quic::QuicTime::Delta::FromMicroseconds(1);
  quic::QuicTime new_deadline = clock_.Now() + kNewDelta;
  alarm->Set(new_deadline);
  EXPECT_TRUE(alarm->IsSet());
  EXPECT_EQ(alarm->deadline(), new_deadline);
  EXPECT_FALSE(delegate->fired());

  // Advancing time for the shorter delay will fire the alarm.
  runner_->FastForwardBy(kNewDelta);

  EXPECT_EQ(quic::QuicTime::Zero() + kNewDelta, clock_.Now());
  EXPECT_FALSE(alarm->IsSet());
  EXPECT_TRUE(delegate->fired());

  delegate->Clear();
  EXPECT_FALSE(delegate->fired());

  // Advancing time for the remaining of the new original delay should not cause
  // the alarm to fire again.
  runner_->FastForwardBy(kDelta - kNewDelta);

  EXPECT_EQ(quic::QuicTime::Zero() + kDelta, clock_.Now());
  EXPECT_FALSE(alarm->IsSet());
  EXPECT_FALSE(delegate->fired());
}

TEST_F(QuicChromiumAlarmFactoryTest, CreateAlarmAndUpdate) {
  TestDelegate* delegate = new TestDelegate();
  std::unique_ptr<quic::QuicAlarm> alarm(alarm_factory_.CreateAlarm(delegate));

  // Set the deadline 1µs in the future.
  constexpr quic::QuicTime::Delta kDelta =
      quic::QuicTime::Delta::FromMicroseconds(1);
  quic::QuicTime deadline = clock_.Now() + kDelta;
  alarm->Set(deadline);
  EXPECT_TRUE(alarm->IsSet());
  EXPECT_EQ(alarm->deadline(), deadline);
  EXPECT_FALSE(delegate->fired());

  // Update the deadline.
  constexpr quic::QuicTime::Delta kNewDelta =
      quic::QuicTime::Delta::FromMicroseconds(3);
  quic::QuicTime new_deadline = clock_.Now() + kNewDelta;
  alarm->Update(new_deadline, quic::QuicTime::Delta::FromMicroseconds(1));
  EXPECT_TRUE(alarm->IsSet());
  EXPECT_EQ(alarm->deadline(), new_deadline);
  EXPECT_FALSE(delegate->fired());

  // Update the alarm with another delta that is not further away from the
  // current deadline than the granularity. The deadline should not change.
  alarm->Update(new_deadline + quic::QuicTime::Delta::FromMicroseconds(1),
                quic::QuicTime::Delta::FromMicroseconds(2));
  EXPECT_TRUE(alarm->IsSet());
  EXPECT_EQ(alarm->deadline(), new_deadline);
  EXPECT_FALSE(delegate->fired());

  // Advancing time for the first delay should not cause the alarm to fire.
  runner_->FastForwardBy(kDelta);

  EXPECT_EQ(quic::QuicTime::Zero() + kDelta, clock_.Now());
  EXPECT_TRUE(alarm->IsSet());
  EXPECT_FALSE(delegate->fired());

  // Advancing time for the remaining of the new delay will fire the alarm.
  runner_->FastForwardBy(kNewDelta - kDelta);

  EXPECT_EQ(quic::QuicTime::Zero() + kNewDelta, clock_.Now());
  EXPECT_FALSE(alarm->IsSet());
  EXPECT_TRUE(delegate->fired());

  // Set the alarm via an update call.
  new_deadline = clock_.Now() + quic::QuicTime::Delta::FromMicroseconds(5);
  alarm->Update(new_deadline, quic::QuicTime::Delta::FromMicroseconds(1));
  EXPECT_TRUE(alarm->IsSet());
  EXPECT_EQ(alarm->deadline(), new_deadline);

  // Update it with an uninitialized time and ensure it's cancelled.
  alarm->Update(quic::QuicTime::Zero(),
                quic::QuicTime::Delta::FromMicroseconds(1));
  EXPECT_FALSE(alarm->IsSet());
}

}  // namespace
}  // namespace net::test
