// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_chromium_alarm_factory.h"

#include "net/quic/test_task_runner.h"
#include "net/third_party/quiche/src/quic/test_tools/mock_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {
namespace {

class TestDelegate : public quic::QuicAlarm::Delegate {
 public:
  TestDelegate() : fired_(false) {}

  void OnAlarm() override { fired_ = true; }

  bool fired() const { return fired_; }
  void Clear() { fired_ = false; }

 private:
  bool fired_;
};

class QuicChromiumAlarmFactoryTest : public ::testing::Test {
 protected:
  QuicChromiumAlarmFactoryTest()
      : runner_(new TestTaskRunner(&clock_)),
        alarm_factory_(runner_.get(), &clock_) {}

  scoped_refptr<TestTaskRunner> runner_;
  QuicChromiumAlarmFactory alarm_factory_;
  quic::MockClock clock_;
};

TEST_F(QuicChromiumAlarmFactoryTest, CreateAlarm) {
  TestDelegate* delegate = new TestDelegate();
  std::unique_ptr<quic::QuicAlarm> alarm(alarm_factory_.CreateAlarm(delegate));

  quic::QuicTime::Delta delta = quic::QuicTime::Delta::FromMicroseconds(1);
  alarm->Set(clock_.Now() + delta);

  // Verify that the alarm task has been posted.
  ASSERT_EQ(1u, runner_->GetPostedTasks().size());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(delta.ToMicroseconds()),
            runner_->GetPostedTasks()[0].delay);

  runner_->RunNextTask();
  EXPECT_EQ(quic::QuicTime::Zero() + delta, clock_.Now());
  EXPECT_TRUE(delegate->fired());
}

TEST_F(QuicChromiumAlarmFactoryTest, CreateAlarmAndCancel) {
  TestDelegate* delegate = new TestDelegate();
  std::unique_ptr<quic::QuicAlarm> alarm(alarm_factory_.CreateAlarm(delegate));

  quic::QuicTime::Delta delta = quic::QuicTime::Delta::FromMicroseconds(1);
  alarm->Set(clock_.Now() + delta);
  alarm->Cancel();

  // The alarm task should still be posted.
  ASSERT_EQ(1u, runner_->GetPostedTasks().size());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(delta.ToMicroseconds()),
            runner_->GetPostedTasks()[0].delay);

  runner_->RunNextTask();
  EXPECT_EQ(quic::QuicTime::Zero() + delta, clock_.Now());
  EXPECT_FALSE(delegate->fired());
}

TEST_F(QuicChromiumAlarmFactoryTest, CreateAlarmAndReset) {
  TestDelegate* delegate = new TestDelegate();
  std::unique_ptr<quic::QuicAlarm> alarm(alarm_factory_.CreateAlarm(delegate));

  quic::QuicTime::Delta delta = quic::QuicTime::Delta::FromMicroseconds(1);
  alarm->Set(clock_.Now() + delta);
  alarm->Cancel();
  quic::QuicTime::Delta new_delta = quic::QuicTime::Delta::FromMicroseconds(3);
  alarm->Set(clock_.Now() + new_delta);

  // The alarm task should still be posted.
  ASSERT_EQ(1u, runner_->GetPostedTasks().size());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(delta.ToMicroseconds()),
            runner_->GetPostedTasks()[0].delay);

  runner_->RunNextTask();
  EXPECT_EQ(quic::QuicTime::Zero() + delta, clock_.Now());
  EXPECT_FALSE(delegate->fired());

  // The alarm task should be posted again.
  ASSERT_EQ(1u, runner_->GetPostedTasks().size());

  runner_->RunNextTask();
  EXPECT_EQ(quic::QuicTime::Zero() + new_delta, clock_.Now());
  EXPECT_TRUE(delegate->fired());
}

TEST_F(QuicChromiumAlarmFactoryTest, CreateAlarmAndResetEarlier) {
  TestDelegate* delegate = new TestDelegate();
  std::unique_ptr<quic::QuicAlarm> alarm(alarm_factory_.CreateAlarm(delegate));

  quic::QuicTime::Delta delta = quic::QuicTime::Delta::FromMicroseconds(3);
  alarm->Set(clock_.Now() + delta);
  alarm->Cancel();
  quic::QuicTime::Delta new_delta = quic::QuicTime::Delta::FromMicroseconds(1);
  alarm->Set(clock_.Now() + new_delta);

  // Both alarm tasks will be posted.
  ASSERT_EQ(2u, runner_->GetPostedTasks().size());

  // The earlier task will execute and will fire the alarm->
  runner_->RunNextTask();
  EXPECT_EQ(quic::QuicTime::Zero() + new_delta, clock_.Now());
  EXPECT_TRUE(delegate->fired());
  delegate->Clear();

  // The latter task is still posted.
  ASSERT_EQ(1u, runner_->GetPostedTasks().size());

  // When the latter task is executed, the weak ptr will be invalid and
  // the alarm will not fire.
  runner_->RunNextTask();
  EXPECT_EQ(quic::QuicTime::Zero() + delta, clock_.Now());
  EXPECT_FALSE(delegate->fired());
}

TEST_F(QuicChromiumAlarmFactoryTest, CreateAlarmAndUpdate) {
  TestDelegate* delegate = new TestDelegate();
  std::unique_ptr<quic::QuicAlarm> alarm(alarm_factory_.CreateAlarm(delegate));

  quic::QuicTime start = clock_.Now();
  quic::QuicTime::Delta delta = quic::QuicTime::Delta::FromMicroseconds(1);
  alarm->Set(clock_.Now() + delta);
  quic::QuicTime::Delta new_delta = quic::QuicTime::Delta::FromMicroseconds(3);
  alarm->Update(clock_.Now() + new_delta,
                quic::QuicTime::Delta::FromMicroseconds(1));

  // The alarm task should still be posted.
  ASSERT_EQ(1u, runner_->GetPostedTasks().size());
  EXPECT_EQ(base::TimeDelta::FromMicroseconds(delta.ToMicroseconds()),
            runner_->GetPostedTasks()[0].delay);

  runner_->RunNextTask();
  EXPECT_EQ(quic::QuicTime::Zero() + delta, clock_.Now());
  EXPECT_FALSE(delegate->fired());

  // Move the alarm forward 1us and ensure it doesn't move forward.
  alarm->Update(clock_.Now() + new_delta,
                quic::QuicTime::Delta::FromMicroseconds(2));

  ASSERT_EQ(1u, runner_->GetPostedTasks().size());
  EXPECT_EQ(
      base::TimeDelta::FromMicroseconds((new_delta - delta).ToMicroseconds()),
      runner_->GetPostedTasks()[0].delay);
  runner_->RunNextTask();
  EXPECT_EQ(start + new_delta, clock_.Now());
  EXPECT_TRUE(delegate->fired());

  // Set the alarm via an update call.
  new_delta = quic::QuicTime::Delta::FromMicroseconds(5);
  alarm->Update(clock_.Now() + new_delta,
                quic::QuicTime::Delta::FromMicroseconds(1));
  EXPECT_TRUE(alarm->IsSet());

  // Update it with an uninitialized time and ensure it's cancelled.
  alarm->Update(quic::QuicTime::Zero(),
                quic::QuicTime::Delta::FromMicroseconds(1));
  EXPECT_FALSE(alarm->IsSet());
}

}  // namespace
}  // namespace test
}  // namespace net
