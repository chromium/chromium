// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/scheduler/public/non_main_thread.h"

#include "third_party/blink/renderer/platform/scheduler/common/scoped_time_source_override.h"

namespace blink::scheduler {
namespace {

using testing::Eq;
using testing::Return;

class MockTimeSource : public ScopedTimeSourceOverride::TimeSource {
 public:
  MockTimeSource(base::TimeTicks ticks, base::Time date)
      : ticks_(ticks), date_(date) {}
  MockTimeSource() = delete;

  base::TimeTicks NowTicks() const override { return ticks_; }
  base::Time Date() const override { return date_; }

 private:
  const base::TimeTicks ticks_;
  const base::Time date_;
};

MATCHER_P(IsCloseTo, reference_time, "") {
  return (reference_time - base::Hours(2) < arg) &&
         (arg < reference_time + base::Hours(2));
}

MATCHER_P(IsSoonAfter, reference_ticks, "") {
  return (reference_ticks <= arg) && (arg < reference_ticks + base::Minutes(5));
}

class ScopedTimeSourceOverrideTest : public testing::Test {
 protected:
  void SetUp() override {
    ticks_at_start_ = base::TimeTicks::Now();
    time_at_start_ = base::Time::Now();
  }

  const base::TimeTicks kTicks = base::TimeTicks() + base::Seconds(42);
  const base::Time kTime = base::Time::FromJsTime(904935600000L);

  MockTimeSource mock_time_source_{kTicks, kTime};

  base::TimeTicks ticks_at_start_;
  base::Time time_at_start_;
};

TEST_F(ScopedTimeSourceOverrideTest, Basic) {
  {
    auto handle = ScopedTimeSourceOverride::CreateDefault(mock_time_source_);
    EXPECT_THAT(base::TimeTicks::Now(), Eq(kTicks));
    EXPECT_THAT(base::Time::Now(), Eq(kTime));
  }
  EXPECT_THAT(base::TimeTicks::Now(), IsSoonAfter(ticks_at_start_));
  EXPECT_THAT(base::Time::Now(), IsCloseTo(time_at_start_));
}

class ThreadedScopedTimeSourceOverrideTest
    : public ScopedTimeSourceOverrideTest {
 protected:
  void SetUp() override {
    ScopedTimeSourceOverrideTest::SetUp();
    thread1_ = NonMainThread::CreateThread(
        ThreadCreationParams(ThreadType::kTestThread)
            .SetThreadNameForTest("Test thread 1"));
    thread2_ = NonMainThread::CreateThread(
        ThreadCreationParams(ThreadType::kTestThread)
            .SetThreadNameForTest("Test thread 2"));
  }

  void TearDown() override {
    thread2_.reset();
    thread1_.reset();
    ScopedTimeSourceOverrideTest::TearDown();
  }

  void RunOnThread(NonMainThread& thread, base::OnceClosure task) {
    base::WaitableEvent completion(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);

    thread.GetTaskRunner()->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](base::OnceClosure task, base::WaitableEvent* completion) {
              std::move(task).Run();
              completion->Signal();
            },
            std::move(task), base::Unretained(&completion)));
    completion.Wait();
  }

  std::unique_ptr<NonMainThread> thread1_;
  std::unique_ptr<NonMainThread> thread2_;
};

TEST_F(ThreadedScopedTimeSourceOverrideTest, OtherThreadDefault) {
  {
    auto handle = ScopedTimeSourceOverride::CreateDefault(mock_time_source_);

    RunOnThread(*thread1_, base::BindLambdaForTesting([&] {
      EXPECT_THAT(base::TimeTicks::Now(), Eq(kTicks));
      EXPECT_THAT(base::Time::Now(), Eq(kTime));
    }));
  }
  RunOnThread(*thread1_, base::BindLambdaForTesting([&] {
    EXPECT_THAT(base::TimeTicks::Now(), IsSoonAfter(ticks_at_start_));
    EXPECT_THAT(base::Time::Now(), IsCloseTo(time_at_start_));
  }));
  RunOnThread(*thread2_, base::BindLambdaForTesting([&] {
    EXPECT_THAT(base::TimeTicks::Now(), IsSoonAfter(ticks_at_start_));
    EXPECT_THAT(base::Time::Now(), IsCloseTo(time_at_start_));
  }));
}

TEST_F(ThreadedScopedTimeSourceOverrideTest, OtherThreadOverride) {
  {
    auto handle =
        ScopedTimeSourceOverride::CreateForCurrentThread(mock_time_source_);

    EXPECT_THAT(base::TimeTicks::Now(), Eq(kTicks));
    EXPECT_THAT(base::Time::Now(), Eq(kTime));

    RunOnThread(*thread1_, base::BindLambdaForTesting([&] {
      EXPECT_THAT(base::TimeTicks::Now(), IsSoonAfter(ticks_at_start_));
      EXPECT_THAT(base::Time::Now(), IsCloseTo(time_at_start_));
    }));
  }
  EXPECT_THAT(base::TimeTicks::Now(), IsSoonAfter(ticks_at_start_));
  EXPECT_THAT(base::Time::Now(), IsCloseTo(time_at_start_));
}

TEST_F(ThreadedScopedTimeSourceOverrideTest, OtherThreadOverride2) {
  {
    MockTimeSource thread1_mock_time(kTicks + base::Minutes(10),
                                     kTime + base::Minutes(10));
    MockTimeSource thread2_mock_time(kTicks + base::Minutes(20),
                                     kTime + base::Minutes(20));

    auto handle = ScopedTimeSourceOverride::CreateDefault(mock_time_source_);
    std::unique_ptr<ScopedTimeSourceOverride> thread1_override;

    RunOnThread(*thread1_, base::BindLambdaForTesting([&] {
      thread1_override =
          ScopedTimeSourceOverride::CreateForCurrentThread(thread1_mock_time);
      EXPECT_THAT(base::TimeTicks::Now(), Eq(kTicks + base::Minutes(10)));
      EXPECT_THAT(base::Time::Now(), Eq(kTime + base::Minutes(10)));
    }));
    EXPECT_THAT(base::TimeTicks::Now(), Eq(kTicks));
    EXPECT_THAT(base::Time::Now(), kTime);
    RunOnThread(*thread2_, base::BindLambdaForTesting([&] {
      EXPECT_THAT(base::TimeTicks::Now(), Eq(kTicks));
      EXPECT_THAT(base::Time::Now(), kTime);
      {
        auto handle =
            ScopedTimeSourceOverride::CreateForCurrentThread(thread2_mock_time);
        EXPECT_THAT(base::TimeTicks::Now(), Eq(kTicks + base::Minutes(20)));
        EXPECT_THAT(base::Time::Now(), Eq(kTime + base::Minutes(20)));
      }
      EXPECT_THAT(base::TimeTicks::Now(), Eq(kTicks));
      EXPECT_THAT(base::Time::Now(), kTime);
    }));
    EXPECT_THAT(base::TimeTicks::Now(), Eq(kTicks));
    EXPECT_THAT(base::Time::Now(), kTime);
    RunOnThread(*thread1_, base::BindLambdaForTesting([&] {
      EXPECT_THAT(base::TimeTicks::Now(), Eq(kTicks + base::Minutes(10)));
      EXPECT_THAT(base::Time::Now(), Eq(kTime + base::Minutes(10)));
      thread1_override.reset();
      EXPECT_THAT(base::TimeTicks::Now(), Eq(kTicks));
      EXPECT_THAT(base::Time::Now(), Eq(kTime));
    }));
  }
  EXPECT_THAT(base::TimeTicks::Now(), IsSoonAfter(ticks_at_start_));
  EXPECT_THAT(base::Time::Now(), IsCloseTo(time_at_start_));
  RunOnThread(*thread1_, base::BindLambdaForTesting([&] {
    EXPECT_THAT(base::TimeTicks::Now(), IsSoonAfter(ticks_at_start_));
    EXPECT_THAT(base::Time::Now(), IsCloseTo(time_at_start_));
  }));
}

}  // namespace
}  // namespace blink::scheduler
