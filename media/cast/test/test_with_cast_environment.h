// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CAST_TEST_TEST_WITH_CAST_ENVIRONMENT_H_
#define MEDIA_CAST_TEST_TEST_WITH_CAST_ENVIRONMENT_H_

#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "media/cast/cast_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class TickClock;
}  // namespace base

namespace media::cast {

// Inherit from this class if a CastEnvironment is needed in a test.
// Use in class hierarchies where inheritance from ::testing::Test at the same
// time is not desirable or possible (for example, when inheriting from
// testing::TestWithParam).
class WithCastEnvironment {
 public:
  WithCastEnvironment(const WithCastEnvironment&) = delete;
  WithCastEnvironment(WithCastEnvironment&&) = delete;
  WithCastEnvironment& operator=(const WithCastEnvironment&) = delete;
  WithCastEnvironment& operator=(WithCastEnvironment&&) = delete;

 protected:
  WithCastEnvironment();
  ~WithCastEnvironment();

  [[nodiscard]] base::RepeatingClosure QuitClosure() {
    return task_environment_.QuitClosure();
  }

  void RunUntilQuit() { task_environment_.RunUntilQuit(); }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  // Only valid for instances using TimeSource::MOCK_TIME.
  void AdvanceClock(base::TimeDelta delta) {
    task_environment_.AdvanceClock(delta);
  }

  [[nodiscard]] scoped_refptr<base::SingleThreadTaskRunner>
  GetMainThreadTaskRunner() {
    return task_environment_.GetMainThreadTaskRunner();
  }

  [[nodiscard]] const base::TickClock* GetMockTickClock() const {
    return task_environment_.GetMockTickClock();
  }

  [[nodiscard]] const base::TimeTicks NowTicks() const {
    return task_environment_.GetMockTickClock()->NowTicks();
  }

  [[nodiscard]] base::test::TaskEnvironment& task_environment() {
    return task_environment_;
  }

  [[nodiscard]] scoped_refptr<CastEnvironment> cast_environment() {
    return cast_environment_;
  }

 private:
  void OnCastEnvironmentDeletion();

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::OnceClosure deletion_cb_;
  scoped_refptr<CastEnvironment> cast_environment_;
};

// Inherit from this class instead of ::testing::Test directly if a
// CastEnvironment is needed in a test.
class TestWithCastEnvironment : public ::testing::Test,
                                public WithCastEnvironment {
 public:
  TestWithCastEnvironment(TestWithCastEnvironment&&) = delete;
  TestWithCastEnvironment(const TestWithCastEnvironment&) = delete;
  TestWithCastEnvironment& operator=(const TestWithCastEnvironment&) = delete;
  TestWithCastEnvironment& operator=(TestWithCastEnvironment&&) = delete;
  ~TestWithCastEnvironment() override;

 protected:
  using WithCastEnvironment::WithCastEnvironment;
};

}  // namespace media::cast

#endif  // MEDIA_CAST_TEST_TEST_WITH_CAST_ENVIRONMENT_H_
