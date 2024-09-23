// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/canvas/canvas2d/identifiability_study_helper.h"

#include <stdint.h>

#include <array>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/privacy_budget/identifiable_token_builder.h"

// GoogleTest macros trigger a bug in IWYU:
// https://github.com/include-what-you-use/include-what-you-use/issues/1546
// IWYU pragma: no_include <string>

namespace blink {

namespace {
constexpr uint64_t max_uint = UINT64_C(0xFFFFFFFFFFFFFFFF);
}

TEST(IdentifiabilityStudyHelperTest, GetTokenTwice) {
  IdentifiabilityStudyHelper helper;
  helper.UpdateBuilder(1246);
  EXPECT_EQ(helper.GetToken(), helper.GetToken());
}

TEST(IdentifiabilityStudyHelperTest, UpdateBuilderAfterGetToken) {
  IdentifiabilityStudyHelper helper1;
  IdentifiabilityStudyHelper helper2;
  helper1.UpdateBuilder(1246);
  helper2.UpdateBuilder(1246);

  helper1.GetToken();

  helper1.UpdateBuilder(52);
  helper2.UpdateBuilder(52);
  EXPECT_EQ(helper1.GetToken(), helper1.GetToken());
}

TEST(IdentifiabilityStudyHelperTest, SameHashAsIdentifiableTokenBuilder_Empty) {
  IdentifiableTokenBuilder builder;
  IdentifiabilityStudyHelper helper;
  EXPECT_EQ(helper.GetToken(), builder.GetToken());
}

TEST(IdentifiabilityStudyHelperTest,
     SameHashAsIdentifiableTokenBuilder_Aligned) {
  std::array<uint64_t, 8> tokens = {0, 1, max_uint, 45, 83, 123, 0, 3567};
  IdentifiabilityStudyHelper helper1;
  IdentifiabilityStudyHelper helper2;
  IdentifiableTokenBuilder builder;
  helper1.UpdateBuilder(tokens[0], tokens[1], tokens[2], tokens[3], tokens[4],
                        tokens[5], tokens[6], tokens[7]);
  for (uint64_t item : tokens) {
    helper2.UpdateBuilder(item);
    builder.AddToken(item);
  }
  EXPECT_EQ(helper1.GetToken(), builder.GetToken());
  EXPECT_EQ(helper2.GetToken(), builder.GetToken());
}

TEST(IdentifiabilityStudyHelperTest,
     SameHashAsIdentifiableTokenBuilder_Unaligned) {
  std::array<uint64_t, 10> tokens = {0,   1, max_uint, 45,       83,
                                     123, 0, 3567,     max_uint, 2};
  IdentifiabilityStudyHelper helper1;
  IdentifiabilityStudyHelper helper2;
  IdentifiableTokenBuilder builder;
  helper1.UpdateBuilder(tokens[0], tokens[1], tokens[2], tokens[3], tokens[4],
                        tokens[5], tokens[6], tokens[7], tokens[8], tokens[9]);
  for (uint64_t item : tokens) {
    helper2.UpdateBuilder(item);
    builder.AddToken(item);
  }
  EXPECT_EQ(helper1.GetToken(), builder.GetToken());
  EXPECT_EQ(helper2.GetToken(), builder.GetToken());
}

}  // namespace blink
