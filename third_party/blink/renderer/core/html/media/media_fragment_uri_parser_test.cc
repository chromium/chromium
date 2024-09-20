// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/media_fragment_uri_parser.h"

#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

struct ParseNPTTimeTestCase {
  std::string test_name;
  std::string_view time_string;
  double expected_time;
  bool expected_result;
};

using ParseNPTTimeTest = ::testing::TestWithParam<ParseNPTTimeTestCase>;

TEST_P(ParseNPTTimeTest, TestParseNPTTime) {
  const ParseNPTTimeTestCase& test_case = GetParam();
  double time = -1;
  size_t offset = 0;

  MediaFragmentURIParser parser(KURL("http://dummy-url.com/"));

  ASSERT_EQ(parser.ParseNPTTime(test_case.time_string, offset, time),
            test_case.expected_result);
  ASSERT_EQ(time, test_case.expected_time);
}

INSTANTIATE_TEST_SUITE_P(
    ParseNPTTimeTests,
    ParseNPTTimeTest,
    ::testing::ValuesIn<ParseNPTTimeTestCase>({
        {"HhMmSsWithOneDigitHh", "1:07:05", 4025, true},
        {"HhMmSsWithTwoDigitsHh", "10:07:05", 36425, true},
        {"HhMmSsWithTwoDigitsHhFractionalSs", "10:07:05.55", 36425.55, true},
        {"MmSsWithTwoDigitsMm", "07:05", 425, true},
        {"MmSsWithThreeDigitsMm", "790:05", -1, false},
        {"MmSsWithTwoDigitMmsFractionalSs", "07:05.7", 425.7, true},
        {"SsWithOneDigitSs", "7", 7, true},
        {"SsWithTwoDigitsSs", "07", 7, true},
        {"SsWithThreeDigitsSs", "123", 123, true},
        {"SsWithTwoDigitsSsFractionalSs", "07.255", 7.255, true},
        {"InvalidCharacters", "1-07-05", -1, false},
        {"HhMmSsInvalidMm", "0:60:00", -1, false},
        {"HhMmSsInvalidSs", "0:07:60", -1, false},
        {"MmSsInvalidMm", "60:00", -1, false},
        {"MmSsInvalidSs", "07:60", -1, false},
    }),
    [](const testing::TestParamInfo<ParseNPTTimeTest::ParamType>& info) {
      return info.param.test_name;
    });
}  // namespace blink
