// Copyright 2021 The Abseil Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstdint>
#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/base/config.h"
#include "absl/base/internal/raw_logging.h"
#include "absl/base/macros.h"
#include "absl/strings/cord.h"
#include "absl/strings/cord_test_helpers.h"
#include "absl/strings/cordz_test_helpers.h"
#include "absl/strings/internal/cordz_functions.h"
#include "absl/strings/internal/cordz_info.h"
#include "absl/strings/internal/cordz_sample_token.h"
#include "absl/strings/internal/cordz_statistics.h"
#include "absl/strings/internal/cordz_update_tracker.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"

#ifdef ABSL_INTERNAL_CORDZ_ENABLED

using testing::Eq;

namespace absl {
ABSL_NAMESPACE_BEGIN

using cord_internal::CordzInfo;
using cord_internal::CordzSampleToken;
using cord_internal::CordzStatistics;
using cord_internal::CordzUpdateTracker;
using Method = CordzUpdateTracker::MethodIdentifier;

// Do not print cord contents, we only care about 'size' perhaps.
// Note that this method must be inside the named namespace.
inline void PrintTo(const Cord& cord, std::ostream* s) {
  if (s) *s << "Cord[" << cord.size() << "]";
}

namespace {

// Returns a string_view value of the specified length
// We do this to avoid 'consuming' large strings in Cord by default.
absl::string_view MakeString(size_t size) {
  thread_local std::string str;
  str = std::string(size, '.');
  return str;
}

absl::string_view MakeString(TestCordSize size) {
  return MakeString(Length(size));
}

std::string TestParamToString(::testing::TestParamInfo<TestCordSize> size) {
  return absl::StrCat("On", ToString(size.param), "Cord");
}

class CordzUpdateTest : public testing::TestWithParam<TestCordSize> {
 public:
  Cord& cord() { return cord_; }

  Method InitialOr(Method method) const {
    return (GetParam() > TestCordSize::kInlined) ? Method::kConstructorString
                                                 : method;
  }

 private:
  CordzSamplingIntervalHelper sample_every_{1};
  Cord cord_{MakeString(GetParam())};
};

INSTANTIATE_TEST_SUITE_P(WithParam, CordzUpdateTest,
                         testing::Values(TestCordSize::kEmpty,
                                         TestCordSize::kInlined,
                                         TestCordSize::kLarge),
                         TestParamToString);

TEST(CordzTest, ConstructSmallString) {
  CordzSamplingIntervalHelper sample_every{1};
  Cord cord(MakeString(TestCordSize::kSmall));
  EXPECT_THAT(cord, HasValidCordzInfoOf(Method::kConstructorString));
}

TEST(CordzTest, ConstructLargeString) {
  CordzSamplingIntervalHelper sample_every{1};
  Cord cord(MakeString(TestCordSize::kLarge));
  EXPECT_THAT(cord, HasValidCordzInfoOf(Method::kConstructorString));
}

TEST(CordzTest, CopyConstruct) {
  CordzSamplingIntervalHelper sample_every{1};
  Cord src = UnsampledCord(MakeString(TestCordSize::kLarge));
  Cord cord(src);
  EXPECT_THAT(cord, HasValidCordzInfoOf(Method::kConstructorCord));
}

TEST(CordzTest, MoveConstruct) {
  CordzSamplingIntervalHelper sample_every{1};
  Cord src(MakeString(TestCordSize::kLarge));
  Cord cord(std::move(src));
  EXPECT_THAT(cord, HasValidCordzInfoOf(Method::kConstructorString));
}

TEST_P(CordzUpdateTest, AssignCord) {
  Cord src = UnsampledCord(MakeString(TestCordSize::kLarge));
  cord() = src;
  EXPECT_THAT(cord(), HasValidCordzInfoOf(InitialOr(Method::kAssignCord)));
}

TEST(CordzTest, AssignInlinedCord) {
  CordzSampleToken token;
  CordzSamplingIntervalHelper sample_every{1};
  Cord cord(MakeString(TestCordSize::kLarge));
  const CordzInfo* info = GetCordzInfoForTesting(cord);
  Cord src = UnsampledCord(MakeString(TestCordSize::kInlined));
  cord = src;
  EXPECT_THAT(GetCordzInfoForTesting(cord), Eq(nullptr));
  EXPECT_FALSE(CordzInfoIsListed(info));
}

TEST(CordzTest, MoveAssignCord) {
  CordzSamplingIntervalHelper sample_every{1};
  Cord cord;
  Cord src(MakeString(TestCordSize::kLarge));
  cord = std::move(src);
  EXPECT_THAT(cord, HasValidCordzInfoOf(Method::kConstructorString));
}

TEST_P(CordzUpdateTest, AppendCord) {
  Cord src = UnsampledCord(MakeString(TestCordSize::kLarge));
  cord().Append(src);
  EXPECT_THAT(cord(), HasValidCordzInfoOf(InitialOr(Method::kAppendCord)));
}

TEST_P(CordzUpdateTest, MoveAppendCord) {
  cord().Append(UnsampledCord(MakeString(TestCordSize::kLarge)));
  EXPECT_THAT(cord(), HasValidCordzInfoOf(InitialOr(Method::kAppendCord)));
}

TEST_P(CordzUpdateTest, PrependCord) {
  Cord src = UnsampledCord(MakeString(TestCordSize::kLarge));
  cord().Prepend(src);
  EXPECT_THAT(cord(), HasValidCordzInfoOf(InitialOr(Method::kPrependCord)));
}

TEST_P(CordzUpdateTest, AppendSmallArray) {
  cord().Append(MakeString(TestCordSize::kSmall));
  EXPECT_THAT(cord(), HasValidCordzInfoOf(InitialOr(Method::kAppendString)));
}

TEST_P(CordzUpdateTest, AppendLargeArray) {
  cord().Append(MakeString(TestCordSize::kLarge));
  EXPECT_THAT(cord(), HasValidCordzInfoOf(InitialOr(Method::kAppendString)));
}

}  // namespace

ABSL_NAMESPACE_END
}  // namespace absl

#endif  // ABSL_INTERNAL_CORDZ_ENABLED
