// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/attribution_utils.h"

#include "services/network/public/mojom/attribution.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {
namespace {

TEST(AttributionSupportTest, HasAttributionOsSupport) {
  const struct {
    mojom::AttributionSupport attribution_support;
    bool expected;
  } kTestCases[] = {
      {mojom::AttributionSupport::kWeb, false},
      {mojom::AttributionSupport::kWebAndOs, true},
      {mojom::AttributionSupport::kOs, true},
      {mojom::AttributionSupport::kNone, false},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(HasAttributionOsSupport(test_case.attribution_support),
              test_case.expected);
  }
}

TEST(AttributionSupportTest, HasAttributionWebSupport) {
  const struct {
    mojom::AttributionSupport attribution_support;
    bool expected;
  } kTestCases[] = {
      {mojom::AttributionSupport::kWeb, true},
      {mojom::AttributionSupport::kWebAndOs, true},
      {mojom::AttributionSupport::kOs, false},
      {mojom::AttributionSupport::kNone, false},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(HasAttributionWebSupport(test_case.attribution_support),
              test_case.expected);
  }
}

TEST(AttributionSupportTest, HasAttributionSupport) {
  const struct {
    mojom::AttributionSupport attribution_support;
    bool expected;
  } kTestCases[] = {
      {mojom::AttributionSupport::kWeb, true},
      {mojom::AttributionSupport::kWebAndOs, true},
      {mojom::AttributionSupport::kOs, true},
      {mojom::AttributionSupport::kNone, false},
  };

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(HasAttributionSupport(test_case.attribution_support),
              test_case.expected);
  }
}

}  // namespace
}  // namespace network
