// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_INTEREST_GROUP_BA_TEST_UTIL_H_
#define CONTENT_BROWSER_INTEREST_GROUP_BA_TEST_UTIL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "url/origin.h"

namespace content {

// Result of extracting join, bid, and win history from the CBOR B&A request.
struct JoinBidWinHistoryForTest {
  struct PrevWin {
    int32_t prev_win_time_seconds;
    std::string ad_render_id;
  };

  JoinBidWinHistoryForTest();
  ~JoinBidWinHistoryForTest();
  JoinBidWinHistoryForTest(JoinBidWinHistoryForTest&);
  JoinBidWinHistoryForTest& operator=(JoinBidWinHistoryForTest&);
  JoinBidWinHistoryForTest(JoinBidWinHistoryForTest&&);
  JoinBidWinHistoryForTest& operator=(JoinBidWinHistoryForTest&&);

  int join_count;
  int bid_count;
  std::vector<PrevWin> prev_wins;
};

// For a given owner, extracts a map from interest group name to the join,
// bid, and win history for that interest group. Crashes if anything goes
// wrong during lookup and extraction.
std::map<std::string, JoinBidWinHistoryForTest> ExtractJoinBidWinHistories(
    std::string_view bna_request,
    const url::Origin& bidder);

}  // namespace content

#endif  // CONTENT_BROWSER_INTEREST_GROUP_BA_TEST_UTIL_H_
