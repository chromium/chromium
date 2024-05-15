// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/user_level_memory_pressure_signal_generator.h"

#include "base/memory/memory_pressure_listener.h"
#include "base/test/test_mock_time_task_runner.h"
#include "build/build_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/renderer/platform/scheduler/public/main_thread_scheduler.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"

namespace blink::user_level_memory_pressure_signal_generator_test {

using testing::_;

namespace {

base::TimeDelta kInertInterval = base::Minutes(5);
base::TimeDelta kMinimumInterval = base::Minutes(10);

}  // namespace

class MockUserLevelMemoryPressureSignalGenerator
    : public UserLevelMemoryPressureSignalGenerator {
 public:
  explicit MockUserLevelMemoryPressureSignalGenerator(
      scoped_refptr<base::TestMockTimeTaskRunner> mock_time_task_runner,
      base::TimeDelta inert_interval,
      base::TimeDelta minimum_interval,
      MainThreadScheduler* main_thread_scheduler)
      : UserLevelMemoryPressureSignalGenerator(
            mock_time_task_runner,
            inert_interval,
            minimum_interval,
            mock_time_task_runner->GetMockTickClock(),
            main_thread_scheduler) {
    ON_CALL(*this, Generate(_))
        .WillByDefault(testing::Invoke(
            this, &MockUserLevelMemoryPressureSignalGenerator::RealGenerate));
  }
  ~MockUserLevelMemoryPressureSignalGenerator() override = default;

  MOCK_METHOD1(Generate, void(base::TimeTicks));

  void RealGenerate(base::TimeTicks) {
    UserLevelMemoryPressureSignalGenerator::Generate(clock_->NowTicks());
  }

  using UserLevelMemoryPressureSignalGenerator::OnRAILModeChanged;
};

class DummyMainThreadScheduler : public MainThreadScheduler {
 public:
  std::unique_ptr<RendererPauseHandle> PauseScheduler() override {
    return nullptr;
  }
  scoped_refptr<base::SingleThreadTaskRunner> NonWakingTaskRunner() override {
    return nullptr;
  }
  AgentGroupScheduler* CreateAgentGroupScheduler() override { return nullptr; }
  AgentGroupScheduler* GetCurrentAgentGroupScheduler() override {
    return nullptr;
  }

  void AddRAILModeObserver(RAILModeObserver*) override {}
  void RemoveRAILModeObserver(RAILModeObserver const* observer) override {}

  void ForEachMainThreadIsolate(
      base::RepeatingCallback<void(v8::Isolate* isolate)> callback) override {}

  v8::Isolate* Isolate() override { return nullptr; }

  void Shutdown() override {}
  bool ShouldYieldForHighPriorityWork() override { return false; }
  void PostIdleTask(const base::Location&, Thread::IdleTask) override {}
  void PostDelayedIdleTask(const base::Location&,
                           base::TimeDelta,
                           Thread::IdleTask) override {}
  void PostNonNestableIdleTask(const base::Location&,
                               Thread::IdleTask) override {}
  scoped_refptr<base::SingleThreadTaskRunner> V8TaskRunner() override {
    return nullptr;
  }
  scoped_refptr<base::SingleThreadTaskRunner> CleanupTaskRunner() override {
    return nullptr;
  }
  base::TimeTicks MonotonicallyIncreasingVirtualTime() override {
    return base::TimeTicks();
  }
  void AddTaskObserver(base::TaskObserver*) override {}
  void RemoveTaskObserver(base::TaskObserver*) override {}
  void SetV8Isolate(v8::Isolate*) override {}
  void ExecuteAfterCurrentTaskForTesting(
      base::OnceClosure on_completion_task,
      ExecuteAfterCurrentTaskRestricted) override {}
  void StartIdlePeriodForTesting() override {}
  void SetRendererBackgroundedForTesting(bool) override {}
};

class UserLevelMemoryPressureSignalGeneratorTest : public testing::Test {
 public:
  UserLevelMemoryPressureSignalGeneratorTest() = default;

  void SetUp() override {
    test_task_runner_ = base::MakeRefCounted<base::TestMockTimeTaskRunner>();

    // Use sync OnMemoryPressure() to count the number of generated memory
    // pressure signals, because SetUpBlinkTestEnvironment() doesn't
    // make async OnMemoryPressure() available.
    // If SequencedTaskRunner::HasCurrentDefault() returns true, async
    // OnMemoryPressure() is available, but the test environment seems not
    // to initialize it.
    memory_pressure_listener_ = std::make_unique<base::MemoryPressureListener>(
        FROM_HERE,
        WTF::BindRepeating(
            [](base::MemoryPressureListener::MemoryPressureLevel) {}),
        WTF::BindRepeating(
            &UserLevelMemoryPressureSignalGeneratorTest::OnSyncMemoryPressure,
            base::Unretained(this)));
    base::MemoryPressureListener::SetNotificationsSuppressed(false);
    memory_pressure_count_ = 0;
  }

  void TearDown() override { memory_pressure_listener_.reset(); }

  void AdvanceClock(base::TimeDelta delta) {
    DCHECK(!delta.is_negative());
    test_task_runner_->FastForwardBy(delta);
  }

  base::TimeTicks NowTicks() { return test_task_runner_->NowTicks(); }

  std::unique_ptr<MockUserLevelMemoryPressureSignalGenerator>
  CreateUserLevelMemoryPressureSignalGenerator(base::TimeDelta inert_interval) {
    return std::make_unique<MockUserLevelMemoryPressureSignalGenerator>(
        test_task_runner_, inert_interval, kMinimumInterval, &dummy_scheduler_);
  }

 protected:
  scoped_refptr<base::TestMockTimeTaskRunner> test_task_runner_;
  DummyMainThreadScheduler dummy_scheduler_;
  std::unique_ptr<base::MemoryPressureListener> memory_pressure_listener_;
  unsigned memory_pressure_count_ = 0;

 private:
  void OnSyncMemoryPressure(base::MemoryPressureListener::MemoryPressureLevel) {
    ++memory_pressure_count_;
  }
};

TEST_F(UserLevelMemoryPressureSignalGeneratorTest,
       GenerateImmediatelyIfInertIntervalIsNegative) {
  std::unique_ptr<MockUserLevelMemoryPressureSignalGenerator> generator(
      CreateUserLevelMemoryPressureSignalGenerator(base::TimeDelta::Min()));

  // Doesn't see whether loading is finished or not.
  generator->OnRAILModeChanged(RAILMode::kLoad);

  EXPECT_CALL(*generator, Generate(_)).Times(1);
  generator->RequestMemoryPressureSignal();
  EXPECT_EQ(1u, memory_pressure_count_);

  AdvanceClock(base::Minutes(1));

  // Since |minimum_interval_| has not passed yet, no more memory pressure
  // signals is generated.
  EXPECT_CALL(*generator, Generate(_)).Times(1);
  generator->RequestMemoryPressureSignal();
  EXPECT_EQ(1u, memory_pressure_count_);

  AdvanceClock(kMinimumInterval - base::Minutes(1));
  generator->OnRAILModeChanged(RAILMode::kAnimation);

  // |minimum_interval_| has passed. Another memory pressure signal is
  // generated.
  EXPECT_CALL(*generator, Generate(_)).Times(1);
  generator->RequestMemoryPressureSignal();
  EXPECT_EQ(2u, memory_pressure_count_);
}

TEST_F(UserLevelMemoryPressureSignalGeneratorTest,
       GenerateImmediatelyNotLoading) {
  std::unique_ptr<MockUserLevelMemoryPressureSignalGenerator> generator(
      CreateUserLevelMemoryPressureSignalGenerator(kInertInterval));

  //            <-1s->
  // Animation --------o
  //                  ^ \
  //                 /   v
  //            Request  Signal
  // (*) inert interval = 5m

  EXPECT_CALL(*generator, Generate(_)).Times(0);
  generator->OnRAILModeChanged(RAILMode::kAnimation);

  AdvanceClock(base::Seconds(1));

  EXPECT_CALL(*generator, Generate(_)).Times(1);
  generator->RequestMemoryPressureSignal();
  EXPECT_EQ(1u, memory_pressure_count_);
}

TEST_F(UserLevelMemoryPressureSignalGeneratorTest,
       GenerateImmediatelyInertIntervalAfterFinishLoading) {
  std::unique_ptr<MockUserLevelMemoryPressureSignalGenerator> generator(
      CreateUserLevelMemoryPressureSignalGenerator(kInertInterval));

  //                    | inert |
  //     <-1s->         <--5m--->
  // Load ---- Animation --------o
  //                            ^ \
  //                           /   v
  //                      Request  Signal
  // (*) inert interval = 5m

  EXPECT_CALL(*generator, Generate(_)).Times(0);
  generator->OnRAILModeChanged(RAILMode::kLoad);

  AdvanceClock(base::Seconds(1));

  generator->OnRAILModeChanged(RAILMode::kAnimation);

  AdvanceClock(kInertInterval);

  EXPECT_CALL(*generator, Generate(_)).Times(1);
  generator->RequestMemoryPressureSignal();
  EXPECT_EQ(1u, memory_pressure_count_);
}

TEST_F(UserLevelMemoryPressureSignalGeneratorTest,
       GenerateInertIntervalAfterFinishLoadingIfRequestedWhileLoading) {
  std::unique_ptr<MockUserLevelMemoryPressureSignalGenerator> generator(
      CreateUserLevelMemoryPressureSignalGenerator(kInertInterval));

  //                             | inert  |
  //     <-1m-> <--5m-->         <---5m--->
  // Load ------------- Animation ---------o
  //           ^                           |
  //           |                           v
  //         Request                    Signal
  // (*) inert interval = 5m

  EXPECT_CALL(*generator, Generate(_)).Times(0);
  generator->OnRAILModeChanged(RAILMode::kLoad);

  AdvanceClock(base::Minutes(1));

  // Request while loading.
  generator->RequestMemoryPressureSignal();
  base::TimeTicks requested_time = NowTicks();

  AdvanceClock(kInertInterval);

  generator->OnRAILModeChanged(RAILMode::kAnimation);

  AdvanceClock(kInertInterval - base::Seconds(1));

  EXPECT_CALL(*generator, Generate(_)).Times(1);
  // kInertInterval has passed after loading was finished.
  AdvanceClock(base::Seconds(1));

  EXPECT_LE(NowTicks() - requested_time, kMinimumInterval);
  EXPECT_EQ(1u, memory_pressure_count_);
}

TEST_F(UserLevelMemoryPressureSignalGeneratorTest,
       GenerateInertIntervalAfterFinishLoadingIfRequestedWhileInert) {
  std::unique_ptr<MockUserLevelMemoryPressureSignalGenerator> generator(
      CreateUserLevelMemoryPressureSignalGenerator(kInertInterval));

  //                      |        inert       |
  //     <--1m-->         <-1m-> <-1m-> <--3m-->
  // Load ------ Animation ---------------------o
  //                            ^      ^        |
  //                            |      |        v
  //                        Request   Request  Signal(once)
  // (*) inert interval = 5m

  EXPECT_CALL(*generator, Generate(_)).Times(0);
  generator->OnRAILModeChanged(RAILMode::kLoad);

  AdvanceClock(base::Minutes(1));

  generator->OnRAILModeChanged(RAILMode::kAnimation);

  AdvanceClock(base::Minutes(1));

  // Request while inert duration.
  generator->RequestMemoryPressureSignal();

  AdvanceClock(base::Minutes(1));

  // Request while inert duration.
  generator->RequestMemoryPressureSignal();

  AdvanceClock(kInertInterval - base::Minutes(2) - base::Seconds(1));

  // Now kInertInterval has passed after loading was finished.
  // Only 1 Generate() is invoked.
  EXPECT_CALL(*generator, Generate(_)).Times(1);
  AdvanceClock(base::Seconds(1));

  EXPECT_EQ(1u, memory_pressure_count_);
}

TEST_F(UserLevelMemoryPressureSignalGeneratorTest,
       GenerateIfLoadingIsRestarted) {
  std::unique_ptr<MockUserLevelMemoryPressureSignalGenerator> generator(
      CreateUserLevelMemoryPressureSignalGenerator(kInertInterval));

  //                      | inert |                    |inert |
  //     <--1m-->         <--2m-->    <--3m-->         <--5m-->
  // Load ------ Animation ------ Load ------ Animation -------o
  //                         ^                                 |
  //                         |                                 v
  //                       Request                          Generate
  //                          <------------ 9m --------------->
  // (*) inert interval = 5m
  //     minimum interval = 10m

  EXPECT_CALL(*generator, Generate(_)).Times(0);
  generator->OnRAILModeChanged(RAILMode::kLoad);

  AdvanceClock(base::Minutes(1));

  generator->OnRAILModeChanged(RAILMode::kAnimation);

  AdvanceClock(base::Minutes(1));

  // Request while inert duration.
  generator->RequestMemoryPressureSignal();
  base::TimeTicks requested_time = NowTicks();

  AdvanceClock(base::Minutes(1));

  // Now start loading.
  generator->OnRAILModeChanged(RAILMode::kLoad);

  AdvanceClock(base::Minutes(3));

  generator->OnRAILModeChanged(RAILMode::kAnimation);

  AdvanceClock(kInertInterval - base::Seconds(1));

  EXPECT_CALL(*generator, Generate(_)).Times(1);

  AdvanceClock(base::Seconds(1));

  // Confirm that the request is not expired.
  EXPECT_LE(NowTicks() - requested_time, kMinimumInterval);
  EXPECT_EQ(1u, memory_pressure_count_);
}

TEST_F(UserLevelMemoryPressureSignalGeneratorTest,
       NoPressureSignalsIfRequestIsExpired) {
  std::unique_ptr<MockUserLevelMemoryPressureSignalGenerator> generator(
      CreateUserLevelMemoryPressureSignalGenerator(kInertInterval));

  //                      | inert |                    |inert |
  //     <--1m-->         <--2m-->    <--5m-->         <--5m-->
  // Load ------ Animation ------ Load ------ Animation -------x
  //                         ^                                 |
  //                         |                              Expired
  //                       Request
  //                          <------------ 11m -------------->
  // (*) inert interval = 5m
  //     minimum interval = 10m

  EXPECT_CALL(*generator, Generate(_)).Times(0);
  generator->OnRAILModeChanged(RAILMode::kLoad);

  AdvanceClock(base::Minutes(1));

  generator->OnRAILModeChanged(RAILMode::kAnimation);

  AdvanceClock(base::Minutes(1));

  // Request while inert duration.
  generator->RequestMemoryPressureSignal();
  base::TimeTicks requested_time = NowTicks();

  AdvanceClock(base::Minutes(1));

  // Now start loading.
  generator->OnRAILModeChanged(RAILMode::kLoad);

  AdvanceClock(base::Minutes(5));

  generator->OnRAILModeChanged(RAILMode::kAnimation);

  AdvanceClock(kInertInterval);

  EXPECT_GT(NowTicks() - requested_time, kMinimumInterval);
  EXPECT_EQ(0u, memory_pressure_count_);
}

TEST_F(UserLevelMemoryPressureSignalGeneratorTest, TwoRequestsAndOneIsExpired) {
  std::unique_ptr<MockUserLevelMemoryPressureSignalGenerator> generator(
      CreateUserLevelMemoryPressureSignalGenerator(kInertInterval));

  //                      |inert |                     |inert |
  //     <--1m-->         <--2m-->    <--5m-->         <--5m-->
  // Load ------ Animation ------ Load ------ Animation -------o
  //                         ^    ^                            |
  //                         |    |                            v
  //                       Request Request                   Signal
  //                               <--------- 10m ------------>
  // (*) inert interval = 5m
  //     minimum interval = 10m

  EXPECT_CALL(*generator, Generate(_)).Times(0);
  generator->OnRAILModeChanged(RAILMode::kLoad);

  AdvanceClock(base::Minutes(1));

  generator->OnRAILModeChanged(RAILMode::kAnimation);

  AdvanceClock(base::Minutes(1));

  // Request while inert duration.
  generator->RequestMemoryPressureSignal();
  base::TimeTicks first_requested_time = NowTicks();

  AdvanceClock(base::Minutes(1));

  generator->RequestMemoryPressureSignal();
  base::TimeTicks second_requested_time = NowTicks();

  // Now start loading.
  generator->OnRAILModeChanged(RAILMode::kLoad);

  AdvanceClock(base::Minutes(5));

  generator->OnRAILModeChanged(RAILMode::kAnimation);

  // The first request is expired after more than |kMinimumInterval| passes.
  base::TimeDelta time_to_expire =
      (first_requested_time + kMinimumInterval) - NowTicks();
  AdvanceClock(time_to_expire);

  // |kInertInterval| passes after loading is finished, memory pressure
  // signal caused by the second request is generated.
  EXPECT_CALL(*generator, Generate(_)).Times(1);
  AdvanceClock(kInertInterval - time_to_expire);

  // Confirm that the second request is not expired.
  EXPECT_LE(NowTicks() - second_requested_time, kMinimumInterval);
  EXPECT_EQ(1u, memory_pressure_count_);
}

TEST_F(UserLevelMemoryPressureSignalGeneratorTest,
       TwoRequestsCauseSignalsAtTheSameTime) {
  std::unique_ptr<MockUserLevelMemoryPressureSignalGenerator> generator(
      CreateUserLevelMemoryPressureSignalGenerator(kInertInterval));

  //              |   minimum interval          |
  //                           |     inert      |
  //     <--1m-->
  // Load ----------- Animation -----------------o
  //             ^                              ^ \
  //             |                             /   v
  //             Request                   Request  Signal
  // (*) inert interval = 5m
  //     minimum interval = 10m

  EXPECT_CALL(*generator, Generate(_)).Times(0);
  generator->OnRAILModeChanged(RAILMode::kLoad);

  AdvanceClock(base::Minutes(1));

  generator->RequestMemoryPressureSignal();

  AdvanceClock(kMinimumInterval - kInertInterval);

  generator->OnRAILModeChanged(RAILMode::kAnimation);

  test_task_runner_->PostDelayedTask(
      FROM_HERE,
      WTF::BindOnce(
          &UserLevelMemoryPressureSignalGenerator::RequestMemoryPressureSignal,
          WTF::UnretainedWrapper(generator.get())),
      kInertInterval);

  EXPECT_CALL(*generator, Generate(_)).Times(2);

  AdvanceClock(kInertInterval);

  // Generate() has been invoked twice, but only one memory pressure signal
  // must be generated.
  EXPECT_EQ(1u, memory_pressure_count_);
}

TEST_F(UserLevelMemoryPressureSignalGeneratorTest,
       DoesNotGenerateSignalDuringInertInterval) {
  std::unique_ptr<MockUserLevelMemoryPressureSignalGenerator> generator(
      CreateUserLevelMemoryPressureSignalGenerator(kInertInterval));

  //                                      PostTask
  //                   PostTask             |--inert interval->
  //                     |-- inert interval-->
  // Load ----- Animation - Load - Animation -x--------------- o
  //        ^                                 |                |
  //        |                                 |                v
  //     Request                          No Signal          Signal
  // (*) inert interval = 5m
  //     minimum interval = 10m

  EXPECT_CALL(*generator, Generate(_)).Times(0);
  generator->OnRAILModeChanged(RAILMode::kLoad);

  AdvanceClock(base::Minutes(1));

  generator->RequestMemoryPressureSignal();

  AdvanceClock(base::Seconds(1));

  generator->OnRAILModeChanged(RAILMode::kAnimation);

  AdvanceClock(base::Seconds(1));

  generator->OnRAILModeChanged(RAILMode::kLoad);

  AdvanceClock(base::Seconds(1));

  generator->OnRAILModeChanged(RAILMode::kAnimation);

  AdvanceClock(kInertInterval - base::Seconds(2));

  EXPECT_CALL(*generator, Generate(_)).Times(1);
  AdvanceClock(base::Seconds(2));

  EXPECT_EQ(1u, memory_pressure_count_);
}

}  // namespace blink::user_level_memory_pressure_signal_generator_test
