// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/android/traffic_stats.h"

#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

TEST(TrafficStatsAndroidTest, BasicsTest) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  EmbeddedTestServer embedded_test_server;
  embedded_test_server.ServeFilesFromDirectory(
      base::FilePath(FILE_PATH_LITERAL("net/data/url_request_unittest")));
  ASSERT_TRUE(embedded_test_server.Start());

  int64_t tx_bytes_before_request = -1;
  int64_t rx_bytes_before_request = -1;
  EXPECT_TRUE(
      android::traffic_stats::GetTotalTxBytes(&tx_bytes_before_request));
  EXPECT_GE(tx_bytes_before_request, 0);
  EXPECT_TRUE(
      android::traffic_stats::GetTotalRxBytes(&rx_bytes_before_request));
  EXPECT_GE(rx_bytes_before_request, 0);

  TestDelegate test_delegate;
  auto context = CreateTestURLRequestContextBuilder()->Build();

  std::unique_ptr<URLRequest> request(
      context->CreateRequest(embedded_test_server.GetURL("/echo.html"),
                             DEFAULT_PRIORITY, &test_delegate));
  request->Start();
  test_delegate.RunUntilComplete();

  // Bytes should increase because of the network traffic.
  int64_t tx_bytes_after_request = -1;
  int64_t rx_bytes_after_request = -1;
  EXPECT_TRUE(android::traffic_stats::GetTotalTxBytes(&tx_bytes_after_request));
  EXPECT_GT(tx_bytes_after_request, tx_bytes_before_request);
  EXPECT_TRUE(android::traffic_stats::GetTotalRxBytes(&rx_bytes_after_request));
  EXPECT_GT(rx_bytes_after_request, rx_bytes_before_request);
}

TEST(TrafficStatsAndroidTest, UIDBasicsTest) {
  base::test::TaskEnvironment task_environment(
      base::test::TaskEnvironment::MainThreadType::IO);

  EmbeddedTestServer embedded_test_server;
  embedded_test_server.ServeFilesFromDirectory(
      base::FilePath(FILE_PATH_LITERAL("net/data/url_request_unittest")));
  ASSERT_TRUE(embedded_test_server.Start());

  int64_t tx_bytes_before_request = -1;
  int64_t rx_bytes_before_request = -1;
  EXPECT_TRUE(
      android::traffic_stats::GetCurrentUidTxBytes(&tx_bytes_before_request));
  EXPECT_GE(tx_bytes_before_request, 0);
  EXPECT_TRUE(
      android::traffic_stats::GetCurrentUidRxBytes(&rx_bytes_before_request));
  EXPECT_GE(rx_bytes_before_request, 0);

  TestDelegate test_delegate;
  auto context = CreateTestURLRequestContextBuilder()->Build();

  std::unique_ptr<URLRequest> request(
      context->CreateRequest(embedded_test_server.GetURL("/echo.html"),
                             DEFAULT_PRIORITY, &test_delegate));
  request->Start();
  test_delegate.RunUntilComplete();

  // Bytes should increase because of the network traffic.
  int64_t tx_bytes_after_request = -1;
  int64_t rx_bytes_after_request = -1;
  EXPECT_TRUE(
      android::traffic_stats::GetCurrentUidTxBytes(&tx_bytes_after_request));
  EXPECT_GT(tx_bytes_after_request, tx_bytes_before_request);
  EXPECT_TRUE(
      android::traffic_stats::GetCurrentUidRxBytes(&rx_bytes_after_request));
  EXPECT_GT(rx_bytes_after_request, rx_bytes_before_request);
}

}  // namespace

}  // namespace net
