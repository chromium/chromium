// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/filtering_details_url_generator.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "net/base/features.h"
#include "net/dns/dns_test_util.h"
#include "net/dns/opt_record_rdata.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_map.h"

namespace net {
namespace {
using ::testing::ElementsAreArray;
using ::testing::IsNull;
using ::testing::NotNull;
using ::testing::SizeIs;

TEST(FilteringDetailsUrlGeneratorTest, ExpandsTemplate) {
  ScopedSetFilteringDetailsUrlGeneratorForTesting scoped_url_gen;
  std::optional<std::string> url =
      FilteringDetailsUrlGenerator::GetInstance().GenerateUrl("example",
                                                              "abc123");

  ASSERT_TRUE(url);
  EXPECT_EQ(*url, "https://resolver.example.com/filtering-incidents/abc123");
}

TEST(FilteringDetailsUrlGeneratorTest, NoRegistryEntryReturnsNullopt) {
  std::optional<std::string> url =
      FilteringDetailsUrlGenerator::GetInstance().GenerateUrl("missing",
                                                              "abc123");
  EXPECT_FALSE(url);
}

TEST(FilteringDetailsUrlGeneratorTest, DisabledFeatureReturnsNullopt) {
  base::test::ScopedFeatureList scoped_feature_list;
  const FilteringDetailsUrlGenerator& generator =
      FilteringDetailsUrlGenerator::GetInstance();
  const auto& registry = generator.GetRegistryForTesting();
  auto it = registry.find("lumen");
  ASSERT_TRUE(it != registry.end());
  ASSERT_TRUE(it->second.feature != nullptr);
  scoped_feature_list.InitAndDisableFeature(*it->second.feature);

  std::optional<std::string> url = generator.GenerateUrl("lumen", "abc123");
  EXPECT_FALSE(url.has_value());
}

TEST(FilteringDetailsUrlGeneratorTest, EnabledFeatureGeneratesUrl) {
  base::test::ScopedFeatureList scoped_feature_list;
  const FilteringDetailsUrlGenerator& generator =
      FilteringDetailsUrlGenerator::GetInstance();
  const auto& registry = generator.GetRegistryForTesting();
  auto it = registry.find("lumen");
  ASSERT_TRUE(it != registry.end());
  ASSERT_TRUE(it->second.feature != nullptr);
  scoped_feature_list.InitAndEnableFeature(*it->second.feature);

  std::optional<std::string> url = generator.GenerateUrl("lumen", "abc123");
  ASSERT_TRUE(url.has_value());
  EXPECT_EQ(*url, "https://lumendatabase.com/notices/abc123");
}
}  // namespace
}  // namespace net
