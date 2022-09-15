// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/keepalive_statistics_recorder.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

TEST(KeepaliveStatisticsRecorderTest, InitialState) {
  KeepaliveStatisticsRecorder r;

  EXPECT_EQ(0, r.num_inflight_requests());
  EXPECT_EQ(0, r.peak_inflight_requests());
  EXPECT_TRUE(r.per_top_level_frame_records().empty());
}

TEST(KeepaliveStatisticsRecorderTest, Register) {
  KeepaliveStatisticsRecorder r;
  const base::UnguessableToken token = base::UnguessableToken::Create();
  r.Register(token);
  EXPECT_EQ(0, r.num_inflight_requests());
  EXPECT_EQ(0, r.peak_inflight_requests());

  const auto& map = r.per_top_level_frame_records();
  EXPECT_EQ(1u, map.size());
  auto it = map.find(token);
  ASSERT_NE(it, map.end());
  EXPECT_EQ(1, it->second.num_registrations);
  EXPECT_EQ(0, it->second.num_inflight_requests);
  EXPECT_EQ(0, it->second.peak_inflight_requests);
}

TEST(KeepaliveStatisticsRecorderTest, Unregister) {
  KeepaliveStatisticsRecorder r;
  const base::UnguessableToken token = base::UnguessableToken::Create();
  r.Register(token);
  EXPECT_FALSE(r.per_top_level_frame_records().empty());
  r.Unregister(token);
  EXPECT_TRUE(r.per_top_level_frame_records().empty());
}

TEST(KeepaliveStatisticsRecorderTest, MultipleRegistration) {
  KeepaliveStatisticsRecorder r;
  const base::UnguessableToken token1 = base::UnguessableToken::Create();
  const base::UnguessableToken token2 = base::UnguessableToken::Create();
  const base::UnguessableToken token3 = base::UnguessableToken::Create();

  r.Register(token1);
  r.Register(token2);
  r.Register(token3);
  r.Register(token1);
  r.Register(token2);

  r.Unregister(token1);
  r.Unregister(token3);

  const auto& map = r.per_top_level_frame_records();
  EXPECT_EQ(2u, map.size());
  auto it1 = map.find(token1);
  auto it2 = map.find(token2);
  auto it3 = map.find(token3);

  EXPECT_NE(it1, map.end());
  EXPECT_EQ(1, it1->second.num_registrations);
  EXPECT_EQ(0, it1->second.num_inflight_requests);
  EXPECT_EQ(0, it1->second.peak_inflight_requests);
  EXPECT_NE(it2, map.end());
  EXPECT_EQ(2, it2->second.num_registrations);
  EXPECT_EQ(0, it2->second.num_inflight_requests);
  EXPECT_EQ(0, it2->second.peak_inflight_requests);
  EXPECT_EQ(it3, map.end());
}

TEST(KeepaliveStatisticsRecorderTest, IssueOneRequest) {
  KeepaliveStatisticsRecorder r;
  const base::UnguessableToken token = base::UnguessableToken::Create();

  r.Register(token);
  r.OnLoadStarted(token, 12);
  {
    const auto& map = r.per_top_level_frame_records();
    EXPECT_EQ(1u, map.size());
    auto it = map.find(token);
    ASSERT_NE(it, map.end());
    EXPECT_EQ(1, it->second.num_registrations);
    EXPECT_EQ(1, it->second.num_inflight_requests);
    EXPECT_EQ(1, it->second.peak_inflight_requests);
    EXPECT_EQ(12, it->second.total_request_size);

    EXPECT_EQ(1, r.num_inflight_requests());
    EXPECT_EQ(1, r.peak_inflight_requests());
  }

  r.OnLoadFinished(token, 12);
  {
    const auto& map = r.per_top_level_frame_records();
    EXPECT_EQ(1u, map.size());
    auto it = map.find(token);
    ASSERT_NE(it, map.end());
    EXPECT_EQ(1, it->second.num_registrations);
    EXPECT_EQ(0, it->second.num_inflight_requests);
    EXPECT_EQ(1, it->second.peak_inflight_requests);
    EXPECT_EQ(0, it->second.total_request_size);

    EXPECT_EQ(0, r.num_inflight_requests());
    EXPECT_EQ(1, r.peak_inflight_requests());
  }
}

TEST(KeepaliveStatisticsRecorderTest, IssueRequests) {
  KeepaliveStatisticsRecorder r;
  const base::UnguessableToken token1 = base::UnguessableToken::Create();
  const base::UnguessableToken token2 = base::UnguessableToken::Create();
  const base::UnguessableToken token3 = base::UnguessableToken::Create();

  r.Register(token1);
  r.Register(token1);
  r.Register(token1);
  r.Register(token2);
  r.Register(token2);

  r.OnLoadStarted(token1, 13);
  r.OnLoadStarted(token1, 5);
  r.OnLoadStarted(token2, 8);
  r.OnLoadStarted(token2, 4);
  r.OnLoadStarted(token2, 82);
  r.OnLoadStarted(token2, 3);
  r.OnLoadStarted(token3, 1);
  r.OnLoadFinished(token2, 4);
  r.OnLoadFinished(token2, 8);
  r.OnLoadFinished(token2, 82);
  r.OnLoadStarted(token2, 13);
  r.OnLoadStarted(token3, 4);
  r.OnLoadStarted(token3, 5);
  r.OnLoadStarted(token3, 6);
  r.OnLoadStarted(token3, 7);
  r.OnLoadStarted(token3, 8);
  r.OnLoadFinished(token3, 6);

  const auto& map = r.per_top_level_frame_records();
  EXPECT_EQ(2u, map.size());
  auto it1 = map.find(token1);
  auto it2 = map.find(token2);
  ASSERT_NE(it1, map.end());
  EXPECT_EQ(3, it1->second.num_registrations);
  EXPECT_EQ(2, it1->second.num_inflight_requests);
  EXPECT_EQ(2, it1->second.peak_inflight_requests);
  EXPECT_EQ(18, it1->second.total_request_size);

  ASSERT_NE(it2, map.end());
  EXPECT_EQ(2, it2->second.num_registrations);
  EXPECT_EQ(2, it2->second.num_inflight_requests);
  EXPECT_EQ(4, it2->second.peak_inflight_requests);
  EXPECT_EQ(16, it2->second.total_request_size);

  EXPECT_EQ(9, r.num_inflight_requests());
  EXPECT_EQ(10, r.peak_inflight_requests());
}

TEST(KeepaliveStatisticsRecorderTest, ProcessReuse) {
  KeepaliveStatisticsRecorder r;
  const base::UnguessableToken token = base::UnguessableToken::Create();

  r.Register(token);
  r.OnLoadStarted(token, 1);
  r.OnLoadStarted(token, 2);
  r.OnLoadStarted(token, 3);
  r.OnLoadFinished(token, 2);
  r.OnLoadFinished(token, 3);
  r.OnLoadFinished(token, 1);
  r.Unregister(token);

  r.Register(token);
  const auto& map = r.per_top_level_frame_records();
  EXPECT_EQ(1u, map.size());
  auto it = map.find(token);
  ASSERT_NE(it, map.end());
  EXPECT_EQ(1, it->second.num_registrations);
  EXPECT_EQ(0, it->second.num_inflight_requests);
  EXPECT_EQ(0, it->second.peak_inflight_requests);
  EXPECT_EQ(0, it->second.total_request_size);
}

}  // namespace

}  // namespace network
