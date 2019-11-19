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
    monitor->bytes_sent_ = 0;
    monitor->bytes_received_ = 0;
    monitor->last_received_ticks_ = base::TimeTicks();
    monitor->last_sent_ticks_ = base::TimeTicks();
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

  base::TimeTicks start = base::TimeTicks::Now();
  uint64_t bytes = 12345;
  monitor->IncrementBytesReceived(bytes);
  EXPECT_EQ(bytes, monitor->GetBytesReceived());
  base::TimeDelta delta = monitor->GetTimeSinceLastReceived();
  EXPECT_LE(base::TimeDelta(), delta);
  EXPECT_GE(base::TimeTicks::Now() - start, delta);
}

TEST_F(NetworkActivityMontiorTest, BytesSent) {
  NetworkActivityMonitor* monitor = NetworkActivityMonitor::GetInstance();

  EXPECT_EQ(0u, monitor->GetBytesSent());

  base::TimeTicks start = base::TimeTicks::Now();
  uint64_t bytes = 12345;
  monitor->IncrementBytesSent(bytes);
  EXPECT_EQ(bytes, monitor->GetBytesSent());
  base::TimeDelta delta = monitor->GetTimeSinceLastSent();
  EXPECT_LE(base::TimeDelta(), delta);
  EXPECT_GE(base::TimeTicks::Now() - start, delta);
}

namespace {

void VerifyBytesReceivedIsMultipleOf(uint64_t bytes) {
  EXPECT_EQ(0u,
            NetworkActivityMonitor::GetInstance()->GetBytesReceived() % bytes);
}

void VerifyBytesSentIsMultipleOf(uint64_t bytes) {
  EXPECT_EQ(0u, NetworkActivityMonitor::GetInstance()->GetBytesSent() % bytes);
}

void IncrementBytesReceived(uint64_t bytes) {
  NetworkActivityMonitor::GetInstance()->IncrementBytesReceived(bytes);
}

void IncrementBytesSent(uint64_t bytes) {
  NetworkActivityMonitor::GetInstance()->IncrementBytesSent(bytes);
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
  uint64_t bytes_sent = UINT64_C(91294998765);
  for (size_t i = 0; i < num_increments; ++i) {
    size_t thread_num = i % threads.size();
    threads[thread_num]->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&IncrementBytesReceived, bytes_received));
    threads[thread_num]->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&IncrementBytesSent, bytes_sent));
    threads[thread_num]->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&VerifyBytesSentIsMultipleOf, bytes_sent));
    threads[thread_num]->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&VerifyBytesReceivedIsMultipleOf, bytes_received));
  }

  threads.clear();

  NetworkActivityMonitor* monitor = NetworkActivityMonitor::GetInstance();
  EXPECT_EQ(num_increments * bytes_received, monitor->GetBytesReceived());
  EXPECT_EQ(num_increments * bytes_sent, monitor->GetBytesSent());
}

}  // namespace test

}  // namespace net
