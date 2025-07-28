// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/isolated_web_apps/types/iwa_version.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "base/types/expected.h"
#include "base/version.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {
namespace {

using ::testing::ElementsAreArray;
using ::testing::HasSubstr;
using IwaVersionParseError = IwaVersion::IwaVersionParseError;

struct IwaVersionTestParam {
  std::string version_string;
  base::expected<std::vector<uint32_t>, IwaVersionParseError>
      expected_components;
};

using IwaVersionTest = testing::TestWithParam<IwaVersionTestParam>;

TEST_P(IwaVersionTest, ParseVersion) {
  const IwaVersionTestParam& param = GetParam();
  base::expected<IwaVersion, IwaVersionParseError> result =
      IwaVersion::Create(param.version_string);

  const auto& expected_result = param.expected_components;

  ASSERT_EQ(result.has_value(), expected_result.has_value());

  if (result.has_value()) {
    // Both result and expected_result have values.
    EXPECT_THAT(result.value()->components(),
                ElementsAreArray(expected_result.value()));
    EXPECT_TRUE(result.value()->IsValid());
  } else {
    // Both result and expected_result have errors.
    EXPECT_EQ(result.error(), expected_result.error());
  }
}

INSTANTIATE_TEST_SUITE_P(
    ValidVersions,
    IwaVersionTest,
    testing::ValuesIn(std::vector<IwaVersionTestParam>{
        {.version_string = "1",
         .expected_components = std::vector<uint32_t>{1}},
        {.version_string = "0",
         .expected_components = std::vector<uint32_t>{0}},
        {.version_string = "1.2",
         .expected_components = std::vector<uint32_t>{1, 2}},
        {.version_string = "1.2.3",
         .expected_components = std::vector<uint32_t>{1, 2, 3}},
        {.version_string = "1.2.3.4",
         .expected_components = std::vector<uint32_t>{1, 2, 3, 4}},
        {.version_string = "0.0.0",
         .expected_components = std::vector<uint32_t>{0, 0, 0}},
        {.version_string = "10.20.30",
         .expected_components = std::vector<uint32_t>{10, 20, 30}},
        {.version_string = "4294967295",
         .expected_components = std::vector<uint32_t>{4294967295U}},
        {.version_string = "4294967295.4294967294.4294967293",
         .expected_components = std::vector<uint32_t>{4294967295U, 4294967294U,
                                                      4294967293U}},
    }));

INSTANTIATE_TEST_SUITE_P(
    InvalidVersions,
    IwaVersionTest,
    testing::ValuesIn(std::vector<IwaVersionTestParam>{
        {.version_string = "",
         .expected_components =
             base::unexpected(IwaVersionParseError::kNoComponents)},
        {.version_string = "  ",
         .expected_components =
             base::unexpected(IwaVersionParseError::kNonDigit)},
        {.version_string = "1.2.3.4.5",
         .expected_components =
             base::unexpected(IwaVersionParseError::kTooManyComponents)},
        // This is bigger than what uint32_t can handle
        {.version_string = "4294967296",
         .expected_components =
             base::unexpected(IwaVersionParseError::kCannotConvertToNumber)},
        {.version_string = "999994294967295.2.3",
         .expected_components =
             base::unexpected(IwaVersionParseError::kCannotConvertToNumber)},
        {.version_string = "1.-2.3",
         .expected_components =
             base::unexpected(IwaVersionParseError::kNonDigit)},
        {.version_string = "1..2.3",
         .expected_components =
             base::unexpected(IwaVersionParseError::kEmptyComponent)},
        {.version_string = ".1",
         .expected_components =
             base::unexpected(IwaVersionParseError::kEmptyComponent)},
        {.version_string = "1.",
         .expected_components =
             base::unexpected(IwaVersionParseError::kEmptyComponent)},
        {.version_string = "1.--2.3",
         .expected_components =
             base::unexpected(IwaVersionParseError::kNonDigit)},
        {.version_string = "1.+2.3",
         .expected_components =
             base::unexpected(IwaVersionParseError::kNonDigit)},
        {.version_string = "a.2.3",
         .expected_components =
             base::unexpected(IwaVersionParseError::kNonDigit)},
        {.version_string = "1.a.3",
         .expected_components =
             base::unexpected(IwaVersionParseError::kNonDigit)},
        {.version_string = "1.2.a",
         .expected_components =
             base::unexpected(IwaVersionParseError::kNonDigit)},
        {.version_string = "1.2.3-a",
         .expected_components =
             base::unexpected(IwaVersionParseError::kNonDigit)},
        {.version_string = "1.2.3+a",
         .expected_components =
             base::unexpected(IwaVersionParseError::kNonDigit)},
        {.version_string = "1.2.3-a+a",
         .expected_components =
             base::unexpected(IwaVersionParseError::kNonDigit)},
        {.version_string = "01.2.3",
         .expected_components =
             base::unexpected(IwaVersionParseError::kLeadingZero)},
        {.version_string = "1.02.3",
         .expected_components =
             base::unexpected(IwaVersionParseError::kLeadingZero)},
        {.version_string = "1.2.03",
         .expected_components =
             base::unexpected(IwaVersionParseError::kLeadingZero)},
        {.version_string = "1.2.0.3",
         .expected_components = std::vector<uint32_t>{1, 2, 0, 3}},
    }));

using IwaVersionGetErrorStringTest =
    ::testing::TestWithParam<std::pair<IwaVersionParseError, std::string>>;

TEST_P(IwaVersionGetErrorStringTest, ConvertsErrorToString) {
  EXPECT_THAT(IwaVersion::GetErrorString(GetParam().first),
              HasSubstr(GetParam().second));
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    IwaVersionGetErrorStringTest,
    ::testing::Values(
        std::make_pair(IwaVersionParseError::kNoComponents,
                       "must consist of at least one number"),
        std::make_pair(IwaVersionParseError::kEmptyComponent,
                       "component may not be empty"),
        std::make_pair(IwaVersionParseError::kNonDigit,
                       "component may only contain digits"),
        std::make_pair(IwaVersionParseError::kLeadingZero,
                       "component may not have leading zeros"),
        std::make_pair(IwaVersionParseError::kTooManyComponents,
                       "may not contain more than"),
        std::make_pair(IwaVersionParseError::kCannotConvertToNumber,
                       "could not be converted into a number")));

}  // namespace
}  // namespace web_app
