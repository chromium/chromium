// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TEST_TEST_WITH_TASK_ENVIRONMENT_H_
#define NET_TEST_TEST_WITH_TASK_ENVIRONMENT_H_

#include <list>
#include <memory>
#include <vector>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "net/test/test_net_log_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class TickClock;
}  // namespace base

namespace net {

class NetTaskScheduler;

// A specialized TaskEnvironment for net/ tests that automatically configures
// the NetTaskScheduler if the kNetTaskScheduler feature is enabled.
class NetTaskEnvironment : public base::test::TaskEnvironment {
 public:
  explicit NetTaskEnvironment(
      base::test::TaskEnvironment::MainThreadType main_thread_type =
          base::test::TaskEnvironment::MainThreadType::DEFAULT,
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::DEFAULT);

  NetTaskEnvironment(const NetTaskEnvironment&) = delete;
  NetTaskEnvironment& operator=(const NetTaskEnvironment&) = delete;

  ~NetTaskEnvironment() override;

 private:
  void Init();

  std::unique_ptr<NetTaskScheduler> scheduler_;
  base::sequence_manager::TaskQueue::Handle default_task_queue_;
};

// Inherit from this class if a TaskEnvironment is needed in a test. Use in
// class hierarchies where inheritance from ::testing::Test at the same time is
// not desirable or possible (for example, when inheriting from PlatformTest at
// the same time).
//
// Do not add a ScopedFeatureList member to classes that inherit from this. It
// leads to flaky crashes. Instead call AddScopedFeatureList() to add scoped
// feature lists while preserving a safe destructor order. Examples:
//   AddScopedFeatureList().InitAndEnableFeature(kFeature);
//   AddScopedFeatureList().InitWithFeatureState(kFeature, GetParam());
class WithTaskEnvironment {
 public:
  WithTaskEnvironment(const WithTaskEnvironment&) = delete;
  WithTaskEnvironment& operator=(const WithTaskEnvironment&) = delete;

 protected:
  // Always uses MainThreadType::IO, `time_source` may optionally be provided
  // to mock time. `disabled_features` may be used to disable features (e.g.
  // features::kNetTaskScheduler) before the task environment is initialized.
  explicit WithTaskEnvironment(
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::DEFAULT,
      std::vector<base::test::FeatureRef> disabled_features = {});

  ~WithTaskEnvironment();

  [[nodiscard]] bool MainThreadIsIdle() const {
    return task_environment_.MainThreadIsIdle();
  }

  [[nodiscard]] base::RepeatingClosure QuitClosure() {
    return task_environment_.QuitClosure();
  }

  void RunUntilQuit() { task_environment_.RunUntilQuit(); }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment_.FastForwardBy(delta);
  }

  void FastForwardUntilNoTasksRemain() {
    task_environment_.FastForwardUntilNoTasksRemain();
  }

  // Only valid for instances using TimeSource::MOCK_TIME.
  void AdvanceClock(base::TimeDelta delta) {
    task_environment_.AdvanceClock(delta);
  }

  [[nodiscard]] const base::TickClock* GetMockTickClock() {
    return task_environment_.GetMockTickClock();
  }

  [[nodiscard]] size_t GetPendingMainThreadTaskCount() const {
    return task_environment_.GetPendingMainThreadTaskCount();
  }

  [[nodiscard]] base::TimeDelta NextMainThreadPendingTaskDelay() const {
    return task_environment_.NextMainThreadPendingTaskDelay();
  }

  // Creates a new ScopedFeatureList object and returns a reference to it. The
  // returned ScopeFeatureList will be destroyed after `task_environment_`,
  // avoiding race conditions. This should be used in preference to subclasses
  // creating their own ScopedFeatureList. Calling AddScopedFeatureList()
  // multiple times is supported and is useful when a subclass of a test fixture
  // also needs to configure some features. Where possible initialize all
  // ScopedFeatureLists before starting any work involving the TaskEnvironment
  // to minimise the risk of race conditions.
  base::test::ScopedFeatureList& AddScopedFeatureList();

 private:
  // Wraps a ScopedFeatureList to disable a vector of features at construction
  // time.
  struct FeatureDisabler {
    base::test::ScopedFeatureList feature_list;
    explicit FeatureDisabler(
        const std::vector<base::test::FeatureRef>& disabled_features);
  };

  // Wraps a std::list<ScopedFeatureList> to provide a guarantee of destruction
  // in reverse order of construction.
  class ScopedFeatureLists {
   public:
    ScopedFeatureLists();
    ~ScopedFeatureLists();

    ScopedFeatureLists(const ScopedFeatureLists&) = delete;
    ScopedFeatureLists& operator=(const ScopedFeatureLists&) = delete;

    base::test::ScopedFeatureList& Emplace();

   private:
    // Not a `std::vector` because it is not safe to move a ScopedFeatureList
    // after it has been initialized.
    std::list<base::test::ScopedFeatureList> lists_;
  };

  FeatureDisabler feature_disabler_;

  // ScopedFeatureList objects for use by subclasses. Some subclasses themselves
  // have subclasses which override further features. For that reason we support
  // multiple ScopedFeatureList objects.
  ScopedFeatureLists scoped_feature_lists_;
  NetTaskEnvironment task_environment_;
  TestNetLogManager net_log_manager_;
};

// Inherit from this class instead of ::testing::Test directly if a
// TaskEnvironment is needed in a test. See the class comment for
// WithTaskEnvironment for what to do instead of constructing your own
// ScopedFeatureList objects.
class TestWithTaskEnvironment : public ::testing::Test,
                                public WithTaskEnvironment {
 public:
  explicit TestWithTaskEnvironment(
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::DEFAULT,
      std::vector<base::test::FeatureRef> disabled_features = {})
      : WithTaskEnvironment(time_source, std::move(disabled_features)) {}

  TestWithTaskEnvironment(const TestWithTaskEnvironment&) = delete;
  TestWithTaskEnvironment& operator=(const TestWithTaskEnvironment&) = delete;

 protected:
  using WithTaskEnvironment::WithTaskEnvironment;
};

}  // namespace net

#endif  // NET_TEST_TEST_WITH_TASK_ENVIRONMENT_H_
