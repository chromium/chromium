// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/html/media/media_fragment_uri_parser.h"

#include <array>
#include <cmath>
#include <initializer_list>
#include <optional>
#include <string_view>

#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

struct ParseNPTTimeTestCase {
  std::string test_name;
  std::string_view time_string;
  std::optional<double> expected_time;
};

using ParseNPTTimeTest = ::testing::TestWithParam<ParseNPTTimeTestCase>;

// Tests ParseNPTTime for all time formats, boundary values, and overflow cases.
TEST_P(ParseNPTTimeTest, TestParseNPTTime) {
  const ParseNPTTimeTestCase& test_case = GetParam();
  constexpr double kInitialTime = -1.0;
  double time = kInitialTime;
  size_t offset = 0;

  MediaFragmentURIParser parser(KURL("http://dummy-url.com/"));

  const bool result = parser.ParseNPTTime(test_case.time_string, offset, time);
  ASSERT_EQ(result, test_case.expected_time.has_value());
  if (result) {
    EXPECT_DOUBLE_EQ(time, *test_case.expected_time);
  } else {
    EXPECT_DOUBLE_EQ(time, kInitialTime);
  }
}

const auto kParseNPTTimeCases = std::to_array<ParseNPTTimeTestCase>({
    {"HhMmSsWithOneDigitHh", "1:07:05", 4025},
    {"HhMmSsWithTwoDigitsHh", "10:07:05", 36425},
    {"HhMmSsWithTwoDigitsHhFractionalSs", "10:07:05.55", 36425.55},
    {"MmSsWithTwoDigitsMm", "07:05", 425},
    {"MmSsWithThreeDigitsMm", "790:05", std::nullopt},
    {"MmSsWithTwoDigitMmsFractionalSs", "07:05.7", 425.7},
    {"SsWithOneDigitSs", "7", 7},
    {"SsWithTwoDigitsSs", "07", 7},
    {"SsWithThreeDigitsSs", "123", 123},
    {"SsWithTwoDigitsSsFractionalSs", "07.255", 7.255},
    {"SsWithTrailingDot", "1.", 1.0},
    {"ConsumeFailAtStart", "abc", std::nullopt},
    {"InvalidCharacters", "1-07-05", std::nullopt},
    {"HhMmSsInvalidMm", "0:60:00", std::nullopt},
    {"HhMmSsInvalidSs", "0:07:60", std::nullopt},
    {"MmSsInvalidMm", "60:00", std::nullopt},
    {"MmSsInvalidSs", "07:60", std::nullopt},
    // Boundary values.
    {"MmSsBoundary", "59:59", 3599},
    {"HhMmSsBoundary", "0:59:59", 3599},
    // INT_MAX seconds: exact as double since INT_MAX < 2^53.
    {"SsIntMax", "2147483647", 2147483647.0},
    // Seconds > INT_MAX: StringToInt fails.
    {"SsIntParseOverflow", "9999999999", std::nullopt},
    // 596524 * 3600 > INT_MAX, but double arithmetic handles it correctly.
    {"HhMmSsLargeHhNoMultiplyOverflow", "596524:00:00", 2147486400.0},
    // HH > INT_MAX: StringToInt fails.
    {"HhMmSsHhIntParseOverflow", "9999999999:00:00", std::nullopt},
});

INSTANTIATE_TEST_SUITE_P(
    ParseNPTTimeTests,
    ParseNPTTimeTest,
    ::testing::ValuesIn(kParseNPTTimeCases),
    [](const testing::TestParamInfo<ParseNPTTimeTest::ParamType>& info) {
      return info.param.test_name;
    });

namespace {

void ExpectMaybeDouble(double actual, const std::optional<double>& expected) {
  if (!expected.has_value()) {
    EXPECT_TRUE(std::isnan(actual));
    return;
  }
  EXPECT_DOUBLE_EQ(actual, *expected);
}

struct UrlTimeParseTestCase {
  std::string test_name;
  const char* url;
  std::optional<double> expected_start;
  std::optional<double> expected_end;
};

using UrlTimeParseTest = ::testing::TestWithParam<UrlTimeParseTestCase>;

// Tests start/end time parsing from full URLs, covering NPT formats and
// fragment parsing edge cases.
TEST_P(UrlTimeParseTest, ParsesStartAndEndTimes) {
  const UrlTimeParseTestCase& test_case = GetParam();
  MediaFragmentURIParser parser(KURL(test_case.url));

  const double start = parser.StartTime();
  const double end = parser.EndTime();

  ExpectMaybeDouble(start, test_case.expected_start);
  ExpectMaybeDouble(end, test_case.expected_end);
}

const auto kUrlTimeParseCases = std::to_array<UrlTimeParseTestCase>({
    {"NptPrefix", "http://example.com/v#t=npt:5,10", 5.0, 10.0},
    {"NptPrefixStartOnly", "http://example.com/v#t=npt:5", 5.0, std::nullopt},
    {"NptPrefixCommaEnd", "http://example.com/v#t=npt:,10", 0.0, 10.0},
    {"CommaEnd", "http://example.com/v#t=,10", 0.0, 10.0},
    {"SecIntParseOverflowYieldsNaN", "http://example.com/v#t=9999999999",
     std::nullopt, std::nullopt},
    {"StartOnlyNoPrefix", "http://example.com/v#t=5", 5.0, std::nullopt},
    {"StartOnlyTrailingDotNoPrefix", "http://example.com/v#t=1.", 1.0,
     std::nullopt},
    {"RangeWithTrailingDotFractions", "http://example.com/v#t=1.,2.", 1.0, 2.0},
    {"StartGreaterThanEndYieldsNaN", "http://example.com/v#t=10,5",
     std::nullopt, std::nullopt},
    {"StartEqualsEndYieldsNaN", "http://example.com/v#t=5,5", std::nullopt,
     std::nullopt},
    {"BareNptPrefixYieldsNaN", "http://example.com/v#t=npt:", std::nullopt,
     std::nullopt},
    {"TrailingCommaYieldsNaN", "http://example.com/v#t=5,", std::nullopt,
     std::nullopt},
    {"ExtraTokenAfterEndYieldsNaN", "http://example.com/v#t=5,10,15",
     std::nullopt, std::nullopt},
    {"MalformedEndTokenYieldsNaN", "http://example.com/v#t=5,abc", std::nullopt,
     std::nullopt},
    {"MalformedStartTokenYieldsNaN", "http://example.com/v#t=abc", std::nullopt,
     std::nullopt},
    {"NoFragmentIdentifierYieldsNaN", "http://example.com/v", std::nullopt,
     std::nullopt},
    {"EmptyFragmentYieldsNaN", "http://example.com/v#", std::nullopt,
     std::nullopt},
    {"ParameterWithoutEqualYieldsNaN", "http://example.com/v#foo", std::nullopt,
     std::nullopt},
    {"EmptyTValueYieldsNaN", "http://example.com/v#t=", std::nullopt,
     std::nullopt},
    {"SegmentWithoutEqualThenValidTYieldsStartOnly",
     "http://example.com/v#foo&t=7", 7.0, std::nullopt},
    {"NonTParameterYieldsNaN", "http://example.com/v#track=audio", std::nullopt,
     std::nullopt},
});

INSTANTIATE_TEST_SUITE_P(
    UrlTimeParseTests,
    UrlTimeParseTest,
    ::testing::ValuesIn(kUrlTimeParseCases),
    [](const testing::TestParamInfo<UrlTimeParseTest::ParamType>& info) {
      return info.param.test_name;
    });

struct DefaultTracksTestCase {
  std::string test_name;
  const char* url;
  std::initializer_list<const char*> expected_tracks;
};

using DefaultTracksTest = ::testing::TestWithParam<DefaultTracksTestCase>;

// Tests that track fragment values are collected into the default tracks list.
TEST_P(DefaultTracksTest, CollectsTrackValues) {
  const DefaultTracksTestCase& test_case = GetParam();
  MediaFragmentURIParser parser(KURL(test_case.url));
  Vector<String> tracks = parser.DefaultTracks();

  ASSERT_EQ(tracks.size(), test_case.expected_tracks.size());
  size_t i = 0;
  for (const char* expected : test_case.expected_tracks) {
    EXPECT_EQ(tracks[i++], expected);
  }
}

const auto kDefaultTracksCases = std::to_array<DefaultTracksTestCase>({
    {"SingleTrack", "http://example.com/v#track=audio", {"audio"}},
    {"MultipleTracks",
     "http://example.com/v#track=audio&track=video",
     {"audio", "video"}},
    {"MixedDimensionsSkipsNonTrack",
     "http://example.com/v#t=5,10&track=audio",
     {"audio"}},
});

INSTANTIATE_TEST_SUITE_P(
    DefaultTracksTests,
    DefaultTracksTest,
    ::testing::ValuesIn(kDefaultTracksCases),
    [](const testing::TestParamInfo<DefaultTracksTest::ParamType>& info) {
      return info.param.test_name;
    });

// Tests that large HH (596524 * 3600 > INT_MAX) is computed correctly via
// double arithmetic rather than overflowing signed integer multiplication.
TEST(MediaFragmentURIParserTest, LargeHhNoMultiplyOverflow) {
  MediaFragmentURIParser parser(
      KURL("http://example.com/v#t=596524:00:00,596524:01:00"));
  EXPECT_DOUBLE_EQ(parser.StartTime(), 2147486400.0);
  EXPECT_DOUBLE_EQ(parser.EndTime(), 2147486460.0);
}

// Tests that when the same dimension appears multiple times, only the last
// valid occurrence is used.
TEST(MediaFragmentURIParserTest, LastTParamWins) {
  MediaFragmentURIParser parser(KURL("http://example.com/v#t=5,10&t=20,30"));
  EXPECT_DOUBLE_EQ(parser.StartTime(), 20.0);
  EXPECT_DOUBLE_EQ(parser.EndTime(), 30.0);
}

// Tests that an invalid URL leaves both times as NaN and default tracks empty.
TEST(MediaFragmentURIParserTest, InvalidUrlYieldsNaN) {
  MediaFragmentURIParser parser{KURL()};
  EXPECT_TRUE(std::isnan(parser.StartTime()));
  EXPECT_TRUE(std::isnan(parser.EndTime()));
  EXPECT_TRUE(parser.DefaultTracks().empty());
}

}  // namespace

}  // namespace blink
