// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/scheduler/responsiveness/jank_monitor.h"

#include "base/callback.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind_test_util.h"
#include "base/test/test_mock_time_task_runner.h"
#include "content/browser/scheduler/responsiveness/native_event_observer.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace responsiveness {

class TestObserver : public JankMonitor::Observer {
 public:
  void OnJankStarted() override { jank_started_called_++; }
  void OnJankStopped() override { jank_stopped_called_++; }

  int jank_started_called() { return jank_started_called_; }
  int jank_stopped_called() { return jank_stopped_called_; }

 private:
  int jank_started_called_ = 0;
  int jank_stopped_called_ = 0;
};

class TestMetricSource : public MetricSource {
 public:
  TestMetricSource(Delegate* delegate)
      : MetricSource(delegate) {}
  ~TestMetricSource() override {}

  std::unique_ptr<NativeEventObserver> CreateNativeEventObserver() override {
    return nullptr;
  }
};

class TestJankMonitor : public JankMonitor {
 public:
  TestJankMonitor() {}

  bool destroy_on_monitor_thread_called() {
    return destroy_on_monitor_thread_called_;
  }

  void SetOnDestroyedCallback(base::OnceClosure on_destroyed) {
    on_destroyed_ = std::move(on_destroyed);
  }

  using JankMonitor::timer_running;

 protected:
  ~TestJankMonitor() override {
    if (on_destroyed_)
      std::move(on_destroyed_).Run();
  }

  std::unique_ptr<MetricSource> CreateMetricSource() override {
    return std::make_unique<TestMetricSource>(this);
  }

  void DestroyOnMonitorThread() override {
    destroy_on_monitor_thread_called_ = true;
    JankMonitor::DestroyOnMonitorThread();
  }

 private:
  bool destroy_on_monitor_thread_called_ = false;
  base::OnceClosure on_destroyed_;
  scoped_refptr<base::TestMockTimeTaskRunner> mock_task_runner_;
};

class JankMonitorTest : public testing::Test {
 public:
  JankMonitorTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~JankMonitorTest() override {}

  void SetUp() override {
    monitor_ = base::MakeRefCounted<TestJankMonitor>();
    monitor_->SetUp();
    monitor_->AddObserver(&test_observer_);
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    if (!monitor_)  // Already teared down.
      return;
    monitor_->RemoveObserver(&test_observer_);
    monitor_->Destroy();
    task_environment_.RunUntilIdle();
    monitor_ = nullptr;
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  scoped_refptr<TestJankMonitor> monitor_;
  TestObserver test_observer_;
  int expected_jank_started_ = 0;
  int expected_jank_stopped_ = 0;
};

// Test life cycle management of the jank monitor: DestroyOnMonitorThread()
// and dtor.
TEST_F(JankMonitorTest, LifeCycle) {
  bool monitor_destroyed = false;
  auto closure =
      base::BindLambdaForTesting([&]() { monitor_destroyed = true; });
  monitor_->SetOnDestroyedCallback(std::move(closure));

  EXPECT_FALSE(monitor_->destroy_on_monitor_thread_called());

  // Test that the monitor thread is destroyed.
  monitor_->RemoveObserver(&test_observer_);
  monitor_->Destroy();
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(monitor_->destroy_on_monitor_thread_called());

  // Release the last reference to TestJankMonitor. Check that it doesn't leak.
  monitor_ = nullptr;
  EXPECT_TRUE(monitor_destroyed);
}

#define VALIDATE_TEST_OBSERVER_CALLS()                                       \
  do {                                                                       \
    EXPECT_EQ(test_observer_.jank_started_called(), expected_jank_started_); \
    EXPECT_EQ(test_observer_.jank_stopped_called(), expected_jank_stopped_); \
  } while (0)

// Test monitor with UI thread janks.
TEST_F(JankMonitorTest, JankUIThread) {
  auto janky_task = [&]() {
    VALIDATE_TEST_OBSERVER_CALLS();
    // This is a janky task that runs for 1.5 seconds.
    expected_jank_started_++;
    task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1500));

    // Monitor should observe that the jank has started.
    VALIDATE_TEST_OBSERVER_CALLS();
    expected_jank_stopped_++;
  };

  // Post a janky task to the UI thread. Number of callback calls should be
  // incremented by 1.
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindLambdaForTesting(janky_task));
  task_environment_.RunUntilIdle();
  VALIDATE_TEST_OBSERVER_CALLS();

  // Post a non janky task. Number of callback calls should remain the same.
  base::PostTask(FROM_HERE, {content::BrowserThread::UI}, base::DoNothing());
  task_environment_.RunUntilIdle();
  VALIDATE_TEST_OBSERVER_CALLS();

  // Post a janky task again. Monitor thread timer should fire again. Number of
  // callback calls should be incremented by 1 again.
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindLambdaForTesting(janky_task));
  task_environment_.RunUntilIdle();
  VALIDATE_TEST_OBSERVER_CALLS();
}

// Test monitor with an IO thread jank.
TEST_F(JankMonitorTest, JankIOThread) {
  auto janky_task = [&]() {
    VALIDATE_TEST_OBSERVER_CALLS();

    // This is a janky task that runs for 1.5 seconds.
    expected_jank_started_++;
    task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1500));

    // Monitor should observe that the jank has started.
    VALIDATE_TEST_OBSERVER_CALLS();

    expected_jank_stopped_++;
  };

  // Post a janky task to the IO thread. This should increment the number of
  // callback calls by 1.
  base::PostTask(FROM_HERE, {content::BrowserThread::IO},
                 base::BindLambdaForTesting(janky_task));
  task_environment_.RunUntilIdle();
  VALIDATE_TEST_OBSERVER_CALLS();
}

// Test monitor with a reentrant UI thread task. The reentrant task shouldn't
// be reported by the monitor.
TEST_F(JankMonitorTest, JankUIThreadReentrant) {
  auto janky_task = [&]() {
    VALIDATE_TEST_OBSERVER_CALLS();

    // This is a janky task that runs for 1.5 seconds.
    expected_jank_started_++;
    task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1500));

    // Monitor should observe that the jank has started.
    VALIDATE_TEST_OBSERVER_CALLS();

    auto nested_janky_task = [&]() {
      // This also janks the current thread.
      task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1500));

      // The callback shouldn't be called.
      VALIDATE_TEST_OBSERVER_CALLS();
    };
    base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                   base::BindLambdaForTesting(nested_janky_task));
    // Spin a nested run loop to run |nested_janky_task|.
    base::RunLoop(base::RunLoop::Type::kNestableTasksAllowed).RunUntilIdle();
    expected_jank_stopped_++;
  };

  // Post a janky task to the UI thread. Number of callback calls should be
  // incremented by 1.
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindLambdaForTesting(janky_task));
  task_environment_.RunUntilIdle();
  VALIDATE_TEST_OBSERVER_CALLS();
}

// Test monitor with overlapping janks on both threads. Only the jank started
// first should be reported.
TEST_F(JankMonitorTest, JankUIAndIOThread) {
  auto janky_task_ui = [&]() {
    expected_jank_started_++;

    // This should trigger the monitor. TestJankMonitor::OnJankStarted() should
    // be called once.
    task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1500));
    VALIDATE_TEST_OBSERVER_CALLS();

    // The IO thread is also janky.
    auto janky_task_io = [&]() {
      // This is a janky task that runs for 1.5 seconds, but shouldn't trigger
      // the monitor.
      task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(1500));
      VALIDATE_TEST_OBSERVER_CALLS();

      // Monitor should observe that the jank has started.
    };
    base::PostTask(FROM_HERE, {content::BrowserThread::IO},
                   base::BindLambdaForTesting(janky_task_io));
    task_environment_.RunUntilIdle();
    // TestJankMonitor::OnJankStopped() shouldn't be called.
    VALIDATE_TEST_OBSERVER_CALLS();

    task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(500));
    expected_jank_stopped_++;
  };
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindLambdaForTesting(janky_task_ui));
  task_environment_.RunUntilIdle();
  // Expect that TestJankMonitor::OnJankStopped() was called.
  VALIDATE_TEST_OBSERVER_CALLS();
}

// Test stopping monitor timer when there is no activity and starting monitor
// timer on new activity.
TEST_F(JankMonitorTest, StartStopTimer) {
  // Activity on the UI thread - timer should be running.
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(base::DoNothing::Once()));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(monitor_->timer_running());

  // 11 seconds passed with no activity - timer should be stopped.
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(11 * 1000));
  EXPECT_FALSE(monitor_->timer_running());

  // Activity on IO thread - timer should be restarted.
  base::PostTask(FROM_HERE, {content::BrowserThread::IO},
                 base::BindOnce(base::DoNothing::Once()));
  task_environment_.RunUntilIdle();
  EXPECT_TRUE(monitor_->timer_running());

  // 11 seconds passed with no activity - timer should be stopped.
  task_environment_.FastForwardBy(base::TimeDelta::FromMilliseconds(11 * 1000));
  EXPECT_FALSE(monitor_->timer_running());
}

class TestJankMonitorShutdownRace : public JankMonitor {
 public:
  TestJankMonitorShutdownRace(base::WaitableEvent* shutdown_on_monitor_thread,
                              base::WaitableEvent* shutdown_on_ui_thread)
      : shutdown_on_monitor_thread_(shutdown_on_monitor_thread),
        shutdown_on_ui_thread_(shutdown_on_ui_thread) {}

  using JankMonitor::timer_running;

 protected:
  ~TestJankMonitorShutdownRace() override = default;

  std::unique_ptr<MetricSource> CreateMetricSource() override {
    return std::make_unique<TestMetricSource>(this);
  }

  void DestroyOnMonitorThread() override {
    JankMonitor::DestroyOnMonitorThread();

    // Posts a task to the UI thread. If MetricSource is still active, this
    // will restart the timer and fail the test.
    base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                   base::BindOnce(base::DoNothing::Once()));

    shutdown_on_monitor_thread_->Signal();
  }

  void FinishDestroyMetricSource() override {
    JankMonitor::FinishDestroyMetricSource();

    shutdown_on_ui_thread_->Signal();
  }

 private:
  base::WaitableEvent* shutdown_on_monitor_thread_;
  base::WaitableEvent* shutdown_on_ui_thread_;
};

// Test that shutdown race with the monitor timer doesn't happen.
TEST(JankMonitorShutdownTest, ShutdownRace_TimerRestarted) {
  content::BrowserTaskEnvironment task_environment;

  // Use WaitableEvent to control the progress of shutdown sequence.
  base::WaitableEvent shutdown_on_monitor_thread;
  base::WaitableEvent shutdown_on_ui_thread;

  scoped_refptr<TestJankMonitorShutdownRace> jank_monitor =
      base::MakeRefCounted<TestJankMonitorShutdownRace>(
          &shutdown_on_monitor_thread, &shutdown_on_ui_thread);
  jank_monitor->SetUp();
  task_environment.RunUntilIdle();

  jank_monitor->Destroy();
  task_environment.RunUntilIdle();

  if (!shutdown_on_monitor_thread.IsSignaled())
    shutdown_on_monitor_thread.Wait();
  task_environment.RunUntilIdle();

  if (!shutdown_on_ui_thread.IsSignaled())
    shutdown_on_ui_thread.Wait();
  task_environment.RunUntilIdle();

  // The monitor thread should be shut down with MetricSource destroyed, i.e.
  // the monitor timer shouldn't be restarted.
  EXPECT_FALSE(jank_monitor->timer_running());
}

class TestJankMonitorShutdownRaceTimerFired : public JankMonitor {
 public:
  TestJankMonitorShutdownRaceTimerFired(
      content::BrowserTaskEnvironment* task_environment)
      : task_environment_(task_environment) {}

  bool monitor_timer_fired() const { return monitor_timer_fired_; }

 protected:
  ~TestJankMonitorShutdownRaceTimerFired() override = default;

  std::unique_ptr<MetricSource> CreateMetricSource() override {
    return std::make_unique<TestMetricSource>(this);
  }

  void FinishDestroyMetricSource() override {
    // Forward by 1 ms to trigger the monitor timer. This shouldn't crash even
    // after MetricSource is destroyed.
    task_environment_->FastForwardBy(base::TimeDelta::FromMilliseconds(1));

    JankMonitor::FinishDestroyMetricSource();
  }

  void OnCheckJankiness() override {
    JankMonitor::OnCheckJankiness();
    monitor_timer_fired_ = true;
  }

 private:
  content::BrowserTaskEnvironment* task_environment_;
  bool monitor_timer_fired_ = false;
};

// Test that the monitor timer shouldn't race with shutdown of MetricSource and
// then crashes.
TEST(JankMonitorShutdownTest, ShutdownRace_TimerFired) {
  content::BrowserTaskEnvironment task_environment(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  scoped_refptr<TestJankMonitorShutdownRaceTimerFired> jank_monitor =
      base::MakeRefCounted<TestJankMonitorShutdownRaceTimerFired>(
          &task_environment);
  jank_monitor->SetUp();
  task_environment.RunUntilIdle();

  // Fast-forward by 499 ms. This shouldn't trigger the monitor timer.
  static constexpr base::TimeDelta kCheckInterval =
      base::TimeDelta::FromMilliseconds(500);
  task_environment.FastForwardBy(kCheckInterval -
                                 base::TimeDelta::FromMilliseconds(1));

  EXPECT_FALSE(jank_monitor->monitor_timer_fired());

  jank_monitor->Destroy();
  task_environment.RunUntilIdle();

  // The timer fires, but we shouldn't crash.
  EXPECT_TRUE(jank_monitor->monitor_timer_fired());
}

#undef VALIDATE_TEST_OBSERVER_CALLS

}  // namespace responsiveness.
}  // namespace content.
