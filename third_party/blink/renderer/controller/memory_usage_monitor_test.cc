// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/controller/memory_usage_monitor.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/testing/unit_test_helpers.h"

namespace blink {

class CountingObserver : public MemoryUsageMonitor::Observer {
 public:
  void OnMemoryPing(MemoryUsage) override { ++count_; }
  int count() const { return count_; }

 private:
  int count_ = 0;
};

class MemoryUsageMonitorTest : public testing::Test {
 public:
  MemoryUsageMonitorTest() = default;

  void SetUp() override {
    monitor_.reset(new MemoryUsageMonitor);
    MemoryUsageMonitor::SetInstanceForTesting(monitor_.get());
  }

  void TearDown() override {
    MemoryUsageMonitor::SetInstanceForTesting(nullptr);
    monitor_.reset();
  }

 private:
  std::unique_ptr<MemoryUsageMonitor> monitor_;
};

TEST_F(MemoryUsageMonitorTest, StartStopMonitor) {
  std::unique_ptr<CountingObserver> observer =
      std::make_unique<CountingObserver>();
  EXPECT_FALSE(MemoryUsageMonitor::Instance().TimerIsActive());
  MemoryUsageMonitor::Instance().AddObserver(observer.get());

  EXPECT_TRUE(MemoryUsageMonitor::Instance().TimerIsActive());
  EXPECT_EQ(0, observer->count());

  test::RunDelayedTasks(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1, observer->count());

  test::RunDelayedTasks(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(2, observer->count());
  MemoryUsageMonitor::Instance().RemoveObserver(observer.get());

  test::RunDelayedTasks(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(2, observer->count());
  EXPECT_FALSE(MemoryUsageMonitor::Instance().TimerIsActive());
}

class OneShotObserver : public CountingObserver {
 public:
  void OnMemoryPing(MemoryUsage usage) override {
    MemoryUsageMonitor::Instance().RemoveObserver(this);
    CountingObserver::OnMemoryPing(usage);
  }
};

TEST_F(MemoryUsageMonitorTest, RemoveObserverFromNotification) {
  std::unique_ptr<OneShotObserver> observer1 =
      std::make_unique<OneShotObserver>();
  std::unique_ptr<CountingObserver> observer2 =
      std::make_unique<CountingObserver>();
  MemoryUsageMonitor::Instance().AddObserver(observer1.get());
  MemoryUsageMonitor::Instance().AddObserver(observer2.get());
  EXPECT_EQ(0, observer1->count());
  EXPECT_EQ(0, observer2->count());
  test::RunDelayedTasks(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1, observer1->count());
  EXPECT_EQ(1, observer2->count());
  test::RunDelayedTasks(base::TimeDelta::FromSeconds(1));
  EXPECT_EQ(1, observer1->count());
  EXPECT_EQ(2, observer2->count());
}

}  // namespace blink
