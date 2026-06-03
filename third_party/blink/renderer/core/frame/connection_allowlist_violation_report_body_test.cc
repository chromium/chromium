// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/connection_allowlist_violation_report_body.h"

#include <set>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

// Test whether ConnectionAllowlistViolationReportBody::MatchId() is a pure
// function, i.e. same input will give same return value. The input values are
// randomly picked values.
TEST(ConnectionAllowlistViolationReportBodyMatchIdTest,
     SameInputGeneratesSameMatchId) {
  String url = "https://example.com/url";
  String connection = "https://destination.com/url";
  Vector<String> allowlist = {"pattern1", "pattern2"};
  V8ConnectionAllowlistDisposition disposition(
      V8ConnectionAllowlistDisposition::Enum::kEnforce);

  EXPECT_EQ(ConnectionAllowlistViolationReportBody(url, connection, allowlist,
                                                   disposition)
                .MatchId(),
            ConnectionAllowlistViolationReportBody(url, connection, allowlist,
                                                   disposition)
                .MatchId());

  url = "https://test.org";
  connection = "https://test2.org";
  allowlist = {"pattern3"};
  disposition = V8ConnectionAllowlistDisposition(
      V8ConnectionAllowlistDisposition::Enum::kReport);

  EXPECT_EQ(ConnectionAllowlistViolationReportBody(url, connection, allowlist,
                                                   disposition)
                .MatchId(),
            ConnectionAllowlistViolationReportBody(url, connection, allowlist,
                                                   disposition)
                .MatchId());
}

TEST(ConnectionAllowlistViolationReportBodyMatchIdTest,
     DifferentInputsGenerateDifferentMatchId) {
  const V8ConnectionAllowlistDisposition enforce(
      V8ConnectionAllowlistDisposition::Enum::kEnforce);
  const V8ConnectionAllowlistDisposition report(
      V8ConnectionAllowlistDisposition::Enum::kReport);

  std::set<unsigned> match_ids;

  // Input 1
  match_ids.insert(ConnectionAllowlistViolationReportBody(
                       "https://example1.com", "https://conn1.com",
                       {"a.com", "b.com"}, enforce)
                       .MatchId());

  // Input 2 (different URL)
  match_ids.insert(ConnectionAllowlistViolationReportBody(
                       "https://example2.com", "https://conn1.com",
                       {"a.com", "b.com"}, enforce)
                       .MatchId());

  // Input 3 (different connection)
  match_ids.insert(ConnectionAllowlistViolationReportBody(
                       "https://example1.com", "https://conn2.com",
                       {"a.com", "b.com"}, enforce)
                       .MatchId());

  // Input 4 (different allowlist)
  match_ids.insert(ConnectionAllowlistViolationReportBody(
                       "https://example1.com", "https://conn1.com",
                       {"a.com", "c.com"}, enforce)
                       .MatchId());

  // Input 5 (different disposition)
  match_ids.insert(ConnectionAllowlistViolationReportBody(
                       "https://example1.com", "https://conn1.com",
                       {"a.com", "b.com"}, report)
                       .MatchId());

  // Ensure the match IDs are all distinct (meaning, the size of the set is
  // equal to the number of IDs we added).
  EXPECT_EQ(match_ids.size(), 5u);
}

TEST(ConnectionAllowlistViolationReportBodyMatchIdTest,
     MatchIdGeneratedShouldNotBeZero) {
  const V8ConnectionAllowlistDisposition enforce(
      V8ConnectionAllowlistDisposition::Enum::kEnforce);

  EXPECT_NE(ConnectionAllowlistViolationReportBody("https://example1.com",
                                                   "https://conn1.com",
                                                   {"a.com", "b.com"}, enforce)
                .MatchId(),
            0u);
}

}  // namespace
}  // namespace blink
