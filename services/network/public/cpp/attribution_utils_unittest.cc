// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/attribution_utils.h"

#include <string>
#include <vector>

#include "net/http/structured_headers.h"
#include "services/network/public/mojom/attribution.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace network {
namespace {

TEST(AttributionSupportTest, GetAttributionSupportHeader) {
  const struct {
    mojom::AttributionSupport attribution_support;
    std::vector<std::string> required_keys;
    std::vector<std::string> prohibited_keys;
  } kTestCases[] = {
      {mojom::AttributionSupport::kWeb, {"web"}, {"os"}},
      {mojom::AttributionSupport::kWebAndOs, {"os", "web"}, {}},
      {mojom::AttributionSupport::kOs, {"os"}, {"web"}},
      {mojom::AttributionSupport::kNone, {}, {"os", "web"}},
  };

  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.attribution_support);

    std::string actual =
        GetAttributionSupportHeader(test_case.attribution_support);

    auto dict = net::structured_headers::ParseDictionary(actual);
    EXPECT_TRUE(dict.has_value());

    for (const auto& key : test_case.required_keys) {
      EXPECT_TRUE(dict->contains(key)) << key;
    }

    for (const auto& key : test_case.prohibited_keys) {
      EXPECT_FALSE(dict->contains(key)) << key;
    }
  }
}

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
