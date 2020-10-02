// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/frame/location_report_body.h"

#include <set>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

class TestLocationReportBody : public LocationReportBody {
 public:
  explicit TestLocationReportBody(
      const String& source_file = g_empty_string,
      base::Optional<uint32_t> line_number = base::nullopt,
      base::Optional<uint32_t> column_number = base::nullopt)
      : LocationReportBody(source_file, line_number, column_number) {}
};

// Test whether LocationReportBody::MatchId() is a pure function, i.e. same
// input will give same return value.
TEST(LocationReportBodyMatchIdTest, SameInputGeneratesSameMatchId) {
  String url = "";
  base::Optional<uint32_t> line = base::nullopt, column = base::nullopt;
  EXPECT_EQ(TestLocationReportBody(url, line, column).MatchId(),
            TestLocationReportBody(url, line, column).MatchId());

  url = "https://example.com";
  line = base::make_optional<uint32_t>(0);
  column = base::make_optional<uint32_t>(0);
  EXPECT_EQ(TestLocationReportBody(url, line, column).MatchId(),
            TestLocationReportBody(url, line, column).MatchId());
}

bool AllDistinct(const std::vector<unsigned>& match_ids) {
  return match_ids.size() ==
         std::set<unsigned>(match_ids.begin(), match_ids.end()).size();
}

const struct {
  const char* url;
  const base::Optional<uint32_t> line_number;
  const base::Optional<uint32_t> column_number;
} kLocationReportBodyInputs[] = {
    {"url", base::nullopt, base::nullopt},
    {"url", 0, base::nullopt},
    {"url", base::nullopt, 0},
    {"url", 0, 0},
    {"url", 1, base::nullopt},
    {"url", base::nullopt, 1},
    {"url", 1, 1},
};

TEST(LocationReportBodyMatchIdTest, DifferentInputsGenerateDifferentMatchId) {
  std::vector<unsigned> match_ids;
  for (const auto& input : kLocationReportBodyInputs) {
    match_ids.push_back(TestLocationReportBody(input.url, input.line_number,
                                               input.column_number)
                            .MatchId());
  }
  EXPECT_TRUE(AllDistinct(match_ids));
}

TEST(LocationReportBodyMatchIdTest, MatchIdGeneratedShouldNotBeZero) {
  std::vector<unsigned> match_ids;
  for (const auto& input : kLocationReportBodyInputs) {
    EXPECT_NE(TestLocationReportBody(input.url, input.line_number,
                                     input.column_number)
                  .MatchId(),
              0u);
  }
}

// When URL is empty, LocationReportBody would call |SourceLocation::Capture()|
// to determine the location, and ignore |line_number| and |column_number|
// specified in constructor params.
TEST(LocationReportBodyMatchIdTest,
     EmptyURLGenerateSameMatchIdRegardlessOfOtherParams) {
  const unsigned empty_hash =
      TestLocationReportBody("", base::nullopt, base::nullopt).MatchId();
  for (const auto& input : kLocationReportBodyInputs) {
    EXPECT_EQ(TestLocationReportBody("", input.line_number, input.column_number)
                  .MatchId(),
              empty_hash);
  }
}

}  // namespace
}  // namespace blink
