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

class FilteringDetailsUrlGeneratorTest : public testing::Test {
 protected:
  FilteringDetailsUrlGeneratorTest() {
    feature_list_.InitAndEnableFeature(net::features::kUseStructuredDnsErrors);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(FilteringDetailsUrlGeneratorTest, ExpandsTemplate) {
  absl::flat_hash_map<std::string, std::string> reg;
  reg["example"] = "https://database.example.com/filtering-incidents/{id}";

  FilteringDetailsUrlGenerator generator(reg);
  std::optional<std::string> url = generator.GenerateUrl("example", "abc123");

  ASSERT_TRUE(url);
  EXPECT_EQ(*url, "https://database.example.com/filtering-incidents/abc123");
}

TEST_F(FilteringDetailsUrlGeneratorTest, NoRegistryEntryReturnsNullopt) {
  absl::flat_hash_map<std::string, std::string> empty_registry;
  FilteringDetailsUrlGenerator generator(empty_registry);

  std::optional<std::string> url = generator.GenerateUrl("missing", "abc123");
  EXPECT_FALSE(url);
}

}  // namespace
}  // namespace net
