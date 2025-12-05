// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/user_level_memory_pressure_signal_generator.h"

#include "base/memory/memory_pressure_listener.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/task_environment.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink::user_level_memory_pressure_signal_generator_test {

using testing::_;

namespace {

base::TimeDelta kInertInterval = base::Minutes(5);
base::TimeDelta kMinimumInterval = base::Minutes(10);

}  // namespace

class UserLevelMemoryPressureSignalGeneratorTest
    : public testing::Test,
      public base::MemoryPressureListener {
 public:
  UserLevelMemoryPressureSignalGeneratorTest() = default;
  ~UserLevelMemoryPressureSignalGeneratorTest() override = default;

  void SetUp() override {
    memory_pressure_listener_registration_ =
        std::make_unique<base::SyncMemoryPressureListenerRegistration>(
            base::MemoryPressureListenerTag::kTest, this);
    base::MemoryPressureListener::SetNotificationsSuppressed(false);
  }

  void TearDown() override { memory_pressure_listener_registration_.reset(); }

  // base::MemoryPressureListener:
  MOCK_METHOD(void, OnMemoryPressure, (base::MemoryPressureLevel), (override));

  void FastForwardBy(base::TimeDelta delta) {
    DCHECK(!delta.is_negative());
    task_environment_.FastForwardBy(delta);
  }

  base::TimeTicks NowTicks() { return task_environment_.NowTicks(); }

  std::unique_ptr<UserLevelMemoryPressureSignalGenerator>
  CreateUserLevelMemoryPressureSignalGenerator(base::TimeDelta inert_interval) {
    return std::make_unique<UserLevelMemoryPressureSignalGenerator>(
        task_environment_.GetMainThreadTaskRunner(), inert_interval,
        kMinimumInterval, task_environment_.main_thread_scheduler());
  }

 protected:
  blink::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  std::unique_ptr<base::SyncMemoryPressureListenerRegistration>
      memory_pressure_listener_registration_;
};

TEST_F(UserLevelMemoryPressureSignalGeneratorTest,
       GenerateImmediatelyNotLoading) {
  std::unique_ptr<UserLevelMemoryPressureSignalGenerator> generator(
      CreateUserLevelMemoryPressureSignalGenerator(kInertInterval));

  //            <-1s->
  // Default ----------o
  //                  ^ \
  //                 /   v
  //            Request  Signal
  // (*) inert interval = 5m

  EXPECT_CALL(*this, OnMemoryPressure(_)).Times(0);
  generator->OnRAILModeChanged(RAILMode::kDefault);

  FastForwardBy(base::Seconds(1));

  EXPECT_CALL(*this, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL));
  generator->RequestMemoryPressureSignal();
}

TEST_F(UserLevelMemoryPressureSignalGeneratorTest,
       GenerateImmediatelyInertIntervalAfterFinishLoading) {
  std::unique_ptr<UserLevelMemoryPressureSignalGenerator> generator(
      CreateUserLevelMemoryPressureSignalGenerator(kInertInterval));

  //                    | inert |
  //     <-1s->         <--5m--->
  // Load ----- Default ---------o
  //                            ^ \
  //                           /   v
  //                      Request  Signal
  // (*) inert interval = 5m

  EXPECT_CALL(*this, OnMemoryPressure(_)).Times(0);
  generator->OnRAILModeChanged(RAILMode::kLoad);

  FastForwardBy(base::Seconds(1));

  generator->OnRAILModeChanged(RAILMode::kDefault);

  FastForwardBy(kInertInterval);

  EXPECT_CALL(*this, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL));
  generator->RequestMemoryPressureSignal();
}

TEST_F(UserLevelMemoryPressureSignalGeneratorTest,
       GenerateInertIntervalAfterFinishLoadingIfRequestedWhileLoading) {
  std::unique_ptr<UserLevelMemoryPressureSignalGenerator> generator(
      CreateUserLevelMemoryPressureSignalGenerator(kInertInterval));

  //                             | inert  |
  //     <-1m-> <--5m-->         <---5m--->
  // Load -------------- Default ----------o
  //           ^                           |
  //           |                           v
  //         Request                    Signal
  // (*) inert interval = 5m

  EXPECT_CALL(*this, OnMemoryPressure(_)).Times(0);
  generator->OnRAILModeChanged(RAILMode::kLoad);

  FastForwardBy(base::Minutes(1));

  // Request while loading.
  generator->RequestMemoryPressureSignal();
  base::TimeTicks requested_time = NowTicks();

  FastForwardBy(kInertInterval);

  generator->OnRAILModeChanged(RAILMode::kDefault);

  FastForwardBy(kInertInterval - base::Seconds(1));

  EXPECT_CALL(*this, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL));
  // kInertInterval has passed after loading was finished.
  FastForwardBy(base::Seconds(1));

  EXPECT_LE(NowTicks() - requested_time, kMinimumInterval);
}

TEST_F(UserLevelMemoryPressureSignalGeneratorTest,
       GenerateInertIntervalAfterFinishLoadingIfRequestedWhileInert) {
  std::unique_ptr<UserLevelMemoryPressureSignalGenerator> generator(
      CreateUserLevelMemoryPressureSignalGenerator(kInertInterval));

  //                      |        inert       |
  //     <--1m-->         <-1m-> <-1m-> <--3m-->
  // Load ------- Default ----------------------o
  //                            ^      ^        |
  //                            |      |        v
  //                        Request   Request  Signal(once)
  // (*) inert interval = 5m

  EXPECT_CALL(*this, OnMemoryPressure(_)).Times(0);
  generator->OnRAILModeChanged(RAILMode::kLoad);

  FastForwardBy(base::Minutes(1));

  generator->OnRAILModeChanged(RAILMode::kDefault);

  FastForwardBy(base::Minutes(1));

  // Request while inert duration.
  generator->RequestMemoryPressureSignal();

  FastForwardBy(base::Minutes(1));

  // Request while inert duration.
  generator->RequestMemoryPressureSignal();

  FastForwardBy(kInertInterval - base::Minutes(2) - base::Seconds(1));

  // Now kInertInterval has passed after loading was finished.
  // Only 1 Generate() is invoked.
  EXPECT_CALL(*this, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL));
  FastForwardBy(base::Seconds(1));
}

TEST_F(UserLevelMemoryPressureSignalGeneratorTest,
       GenerateIfLoadingIsRestarted) {
  std::unique_ptr<UserLevelMemoryPressureSignalGenerator> generator(
      CreateUserLevelMemoryPressureSignalGenerator(kInertInterval));

  //                      | inert |                    |inert |
  //     <--1m-->         <--2m-->    <--3m-->         <--5m-->
  // Load ------- Default ------- Load ------- Default --------o
  //                         ^                                 |
  //                         |                                 v
  //                       Request                          Generate
  //                          <------------ 9m --------------->
  // (*) inert interval = 5m
  //     minimum interval = 10m

  EXPECT_CALL(*this, OnMemoryPressure(_)).Times(0);
  generator->OnRAILModeChanged(RAILMode::kLoad);

  FastForwardBy(base::Minutes(1));

  generator->OnRAILModeChanged(RAILMode::kDefault);

  FastForwardBy(base::Minutes(1));

  // Request while inert duration.
  generator->RequestMemoryPressureSignal();
  base::TimeTicks requested_time = NowTicks();

  FastForwardBy(base::Minutes(1));

  // Now start loading.
  generator->OnRAILModeChanged(RAILMode::kLoad);

  FastForwardBy(base::Minutes(3));

  generator->OnRAILModeChanged(RAILMode::kDefault);

  FastForwardBy(kInertInterval - base::Seconds(1));

  EXPECT_CALL(*this, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL));

  FastForwardBy(base::Seconds(1));

  // Confirm that the request is not expired.
  EXPECT_LE(NowTicks() - requested_time, kMinimumInterval);
}

TEST_F(UserLevelMemoryPressureSignalGeneratorTest,
       NoPressureSignalsIfRequestIsExpired) {
  std::unique_ptr<UserLevelMemoryPressureSignalGenerator> generator(
      CreateUserLevelMemoryPressureSignalGenerator(kInertInterval));

  //                      | inert |                    |inert |
  //     <--1m-->         <--2m-->    <--5m-->         <--5m-->
  // Load ------- Default ------- Load ------- Default --------x
  //                         ^                                 |
  //                         |                              Expired
  //                       Request
  //                          <------------ 11m -------------->
  // (*) inert interval = 5m
  //     minimum interval = 10m

  EXPECT_CALL(*this, OnMemoryPressure(_)).Times(0);
  generator->OnRAILModeChanged(RAILMode::kLoad);

  FastForwardBy(base::Minutes(1));

  generator->OnRAILModeChanged(RAILMode::kDefault);

  FastForwardBy(base::Minutes(1));

  // Request while inert duration.
  generator->RequestMemoryPressureSignal();
  base::TimeTicks requested_time = NowTicks();

  FastForwardBy(base::Minutes(1));

  // Now start loading.
  generator->OnRAILModeChanged(RAILMode::kLoad);

  FastForwardBy(base::Minutes(5));

  generator->OnRAILModeChanged(RAILMode::kDefault);

  FastForwardBy(kInertInterval);

  EXPECT_GT(NowTicks() - requested_time, kMinimumInterval);
}

TEST_F(UserLevelMemoryPressureSignalGeneratorTest, TwoRequestsAndOneIsExpired) {
  std::unique_ptr<UserLevelMemoryPressureSignalGenerator> generator(
      CreateUserLevelMemoryPressureSignalGenerator(kInertInterval));

  //                      |inert |                     |inert |
  //     <--1m-->         <--2m-->    <--5m-->         <--5m-->
  // Load ------- Default ------- Load ------- Default --------o
  //                         ^    ^                            |
  //                         |    |                            v
  //                       Request Request                   Signal
  //                               <--------- 10m ------------>
  // (*) inert interval = 5m
  //     minimum interval = 10m

  EXPECT_CALL(*this, OnMemoryPressure(_)).Times(0);
  generator->OnRAILModeChanged(RAILMode::kLoad);

  FastForwardBy(base::Minutes(1));

  generator->OnRAILModeChanged(RAILMode::kDefault);

  FastForwardBy(base::Minutes(1));

  // Request while inert duration.
  generator->RequestMemoryPressureSignal();
  base::TimeTicks first_requested_time = NowTicks();

  FastForwardBy(base::Minutes(1));

  generator->RequestMemoryPressureSignal();
  base::TimeTicks second_requested_time = NowTicks();

  // Now start loading.
  generator->OnRAILModeChanged(RAILMode::kLoad);

  FastForwardBy(base::Minutes(5));

  generator->OnRAILModeChanged(RAILMode::kDefault);

  // The first request is expired after more than |kMinimumInterval| passes.
  base::TimeDelta time_to_expire =
      (first_requested_time + kMinimumInterval) - NowTicks();
  FastForwardBy(time_to_expire);

  // |kInertInterval| passes after loading is finished, memory pressure
  // signal caused by the second request is generated.
  EXPECT_CALL(*this, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL));
  FastForwardBy(kInertInterval - time_to_expire);

  // Confirm that the second request is not expired.
  EXPECT_LE(NowTicks() - second_requested_time, kMinimumInterval);
}

TEST_F(UserLevelMemoryPressureSignalGeneratorTest,
       TwoRequestsCauseSignalsAtTheSameTime) {
  std::unique_ptr<UserLevelMemoryPressureSignalGenerator> generator(
      CreateUserLevelMemoryPressureSignalGenerator(kInertInterval));

  //              |   minimum interval          |
  //                           |     inert      |
  //     <--1m-->
  // Load ------------ Default ------------------o
  //             ^                              ^ \
  //             |                             /   v
  //             Request                   Request  Signal
  // (*) inert interval = 5m
  //     minimum interval = 10m

  EXPECT_CALL(*this, OnMemoryPressure(_)).Times(0);
  generator->OnRAILModeChanged(RAILMode::kLoad);

  FastForwardBy(base::Minutes(1));

  generator->RequestMemoryPressureSignal();

  FastForwardBy(kMinimumInterval - kInertInterval);

  generator->OnRAILModeChanged(RAILMode::kDefault);

  task_environment_.GetMainThreadTaskRunner()->PostDelayedTask(
      FROM_HERE,
      BindOnce(
          &UserLevelMemoryPressureSignalGenerator::RequestMemoryPressureSignal,
          UnretainedWrapper(generator.get())),
      kInertInterval);

  EXPECT_CALL(*this, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL));
  FastForwardBy(kInertInterval);
}

TEST_F(UserLevelMemoryPressureSignalGeneratorTest,
       DoesNotGenerateSignalDuringInertInterval) {
  std::unique_ptr<UserLevelMemoryPressureSignalGenerator> generator(
      CreateUserLevelMemoryPressureSignalGenerator(kInertInterval));

  //                                      PostTask
  //                   PostTask             |--inert interval->
  //                     |-- inert interval-->
  // Load ------ Default -- Load -- Default --x--------------- o
  //        ^                                 |                |
  //        |                                 |                v
  //     Request                          No Signal          Signal
  // (*) inert interval = 5m
  //     minimum interval = 10m

  EXPECT_CALL(*this, OnMemoryPressure(_)).Times(0);
  generator->OnRAILModeChanged(RAILMode::kLoad);

  FastForwardBy(base::Minutes(1));

  generator->RequestMemoryPressureSignal();

  FastForwardBy(base::Seconds(1));

  generator->OnRAILModeChanged(RAILMode::kDefault);

  FastForwardBy(base::Seconds(1));

  generator->OnRAILModeChanged(RAILMode::kLoad);

  FastForwardBy(base::Seconds(1));

  generator->OnRAILModeChanged(RAILMode::kDefault);

  FastForwardBy(kInertInterval - base::Seconds(2));

  EXPECT_CALL(*this, OnMemoryPressure(base::MEMORY_PRESSURE_LEVEL_CRITICAL));
  FastForwardBy(base::Seconds(2));
}

}  // namespace blink::user_level_memory_pressure_signal_generator_test
