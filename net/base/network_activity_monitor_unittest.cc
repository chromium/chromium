// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_activity_monitor.h"

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {

namespace test {

class NetworkActivityMonitorPeer {
 public:
  static void ResetMonitor() {
    NetworkActivityMonitor* monitor = NetworkActivityMonitor::GetInstance();
    base::AutoLock lock(monitor->lock_);
    monitor->bytes_received_ = 0;
  }
};


class NetworkActivityMontiorTest : public testing::Test {
 public:
  NetworkActivityMontiorTest() {
    NetworkActivityMonitorPeer::ResetMonitor();
  }
};

TEST_F(NetworkActivityMontiorTest, GetInstance) {
  NetworkActivityMonitor* monitor = NetworkActivityMonitor::GetInstance();
  EXPECT_TRUE(monitor != nullptr);
  EXPECT_TRUE(monitor == NetworkActivityMonitor::GetInstance());
}

TEST_F(NetworkActivityMontiorTest, BytesReceived) {
  NetworkActivityMonitor* monitor = NetworkActivityMonitor::GetInstance();

  EXPECT_EQ(0u, monitor->GetBytesReceived());

  uint64_t bytes = 12345;
  monitor->IncrementBytesReceived(bytes);
  EXPECT_EQ(bytes, monitor->GetBytesReceived());
}

namespace {

void VerifyBytesReceivedIsMultipleOf(uint64_t bytes) {
  EXPECT_EQ(0u,
            NetworkActivityMonitor::GetInstance()->GetBytesReceived() % bytes);
}

void IncrementBytesReceived(uint64_t bytes) {
  NetworkActivityMonitor::GetInstance()->IncrementBytesReceived(bytes);
}

}  // namespace

TEST_F(NetworkActivityMontiorTest, Threading) {
  std::vector<std::unique_ptr<base::Thread>> threads;
  for (size_t i = 0; i < 3; ++i) {
    threads.push_back(std::make_unique<base::Thread>(base::NumberToString(i)));
    ASSERT_TRUE(threads.back()->Start());
  }

  size_t num_increments = 157;
  uint64_t bytes_received = UINT64_C(7294954321);
  for (size_t i = 0; i < num_increments; ++i) {
    size_t thread_num = i % threads.size();
    threads[thread_num]->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&IncrementBytesReceived, bytes_received));
    threads[thread_num]->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&VerifyBytesReceivedIsMultipleOf, bytes_received));
  }

  threads.clear();

  NetworkActivityMonitor* monitor = NetworkActivityMonitor::GetInstance();
  EXPECT_EQ(num_increments * bytes_received, monitor->GetBytesReceived());
}

}  // namespace test

}  // namespace net
