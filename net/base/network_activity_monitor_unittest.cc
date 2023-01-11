// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/network_activity_monitor.h"

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net::test {

class NetworkActivityMontiorTest : public testing::Test {
 public:
  NetworkActivityMontiorTest() {
    activity_monitor::ResetBytesReceivedForTesting();
  }
};

TEST_F(NetworkActivityMontiorTest, BytesReceived) {
  EXPECT_EQ(0u, activity_monitor::GetBytesReceived());

  uint64_t bytes = 12345;
  activity_monitor::IncrementBytesReceived(bytes);
  EXPECT_EQ(bytes, activity_monitor::GetBytesReceived());
}

namespace {

void VerifyBytesReceivedIsMultipleOf(uint64_t bytes) {
  EXPECT_EQ(0u, activity_monitor::GetBytesReceived() % bytes);
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
        FROM_HERE, base::BindOnce(&activity_monitor::IncrementBytesReceived,
                                  bytes_received));
    threads[thread_num]->task_runner()->PostTask(
        FROM_HERE,
        base::BindOnce(&VerifyBytesReceivedIsMultipleOf, bytes_received));
  }

  threads.clear();

  EXPECT_EQ(num_increments * bytes_received,
            activity_monitor::GetBytesReceived());
}

}  // namespace net::test
