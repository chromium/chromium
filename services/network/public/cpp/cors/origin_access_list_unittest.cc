// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/cors/origin_access_list.h"
#include "services/network/public/mojom/cors.mojom.h"

#include <memory>

#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace network {

namespace cors {

namespace {

// OriginAccessListTest is a out of blink version of blink::SecurityPolicyTest,
// but it contains only tests for the allow/block lists management.
class OriginAccessListTest : public testing::Test {
 public:
  OriginAccessListTest()
      : https_example_origin_(url::Origin::Create(GURL("https://example.com"))),
        https_sub_example_origin_(
            url::Origin::Create(GURL("https://sub.example.com"))),
        http_example_origin_(url::Origin::Create(GURL("http://example.com"))),
        https_google_origin_(url::Origin::Create(GURL("https://google.com"))),
        source_origin_(url::Origin::Create(GURL("https://chromium.org"))) {}

  ~OriginAccessListTest() override = default;

 protected:
  const url::Origin& https_example_origin() const {
    return https_example_origin_;
  }
  const url::Origin& https_sub_example_origin() const {
    return https_sub_example_origin_;
  }
  const url::Origin& http_example_origin() const {
    return http_example_origin_;
  }
  const url::Origin& https_google_origin() const {
    return https_google_origin_;
  }
  bool IsAllowed(const url::Origin& destination_origin) const {
    return list_.IsAllowed(source_origin_, destination_origin.GetURL());
  }
  void SetAllowListEntry(const std::string& protocol,
                         const std::string& host,
                         bool allow_subdomains) {
    std::vector<mojom::CorsOriginPatternPtr> patterns;
    patterns.push_back(mojom::CorsOriginPattern::New(
        protocol, host, allow_subdomains,
        network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority));
    list_.SetAllowListForOrigin(source_origin_, patterns);
  }
  void AddAllowListEntry(
      const std::string& protocol,
      const std::string& host,
      bool allow_subdomains,
      const network::mojom::CORSOriginAccessMatchPriority priority) {
    list_.AddAllowListEntryForOrigin(source_origin_, protocol, host,
                                     allow_subdomains, priority);
  }
  void SetBlockListEntry(const std::string& protocol,
                         const std::string& host,
                         bool allow_subdomains) {
    std::vector<mojom::CorsOriginPatternPtr> patterns;
    patterns.push_back(mojom::CorsOriginPattern::New(
        protocol, host, allow_subdomains,
        network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority));
    list_.SetBlockListForOrigin(source_origin_, patterns);
  }
  void AddBlockListEntry(
      const std::string& protocol,
      const std::string& host,
      bool allow_subdomains,
      const network::mojom::CORSOriginAccessMatchPriority priority) {
    list_.AddBlockListEntryForOrigin(source_origin_, protocol, host,
                                     allow_subdomains, priority);
  }
  void ResetLists() {
    std::vector<mojom::CorsOriginPatternPtr> patterns;
    list_.SetAllowListForOrigin(source_origin_, patterns);
    list_.SetBlockListForOrigin(source_origin_, patterns);
  }

 private:
  url::Origin https_example_origin_;
  url::Origin https_sub_example_origin_;
  url::Origin http_example_origin_;
  url::Origin https_google_origin_;

  url::Origin source_origin_;

  OriginAccessList list_;

  DISALLOW_COPY_AND_ASSIGN(OriginAccessListTest);
};

TEST_F(OriginAccessListTest, IsAccessAllowed) {
  // By default, no access should be allowed.
  EXPECT_FALSE(IsAllowed(https_example_origin()));
  EXPECT_FALSE(IsAllowed(https_sub_example_origin()));
  EXPECT_FALSE(IsAllowed(http_example_origin()));

  // Adding access for https://example.com should work, but should not grant
  // access to subdomains or other schemes.
  SetAllowListEntry("https", "example.com", false);
  EXPECT_TRUE(IsAllowed(https_example_origin()));
  EXPECT_FALSE(IsAllowed(https_sub_example_origin()));
  EXPECT_FALSE(IsAllowed(http_example_origin()));

  // Clearing the map should revoke all special access.
  ResetLists();
  EXPECT_FALSE(IsAllowed(https_example_origin()));
  EXPECT_FALSE(IsAllowed(https_sub_example_origin()));
  EXPECT_FALSE(IsAllowed(http_example_origin()));

  // Adding an entry that matches subdomains should grant access to any
  // subdomains.
  AddAllowListEntry(
      "https", "example.com", true,
      network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority);
  EXPECT_TRUE(IsAllowed(https_example_origin()));
  EXPECT_TRUE(IsAllowed(https_sub_example_origin()));
  EXPECT_FALSE(IsAllowed(http_example_origin()));
}

TEST_F(OriginAccessListTest, IsAccessAllowedWildCard) {
  // An empty domain that matches subdomains results in matching every domain.
  SetAllowListEntry("https", "", true);
  EXPECT_TRUE(IsAllowed(https_example_origin()));
  EXPECT_TRUE(IsAllowed(https_google_origin()));
  EXPECT_FALSE(IsAllowed(http_example_origin()));
}

TEST_F(OriginAccessListTest, IsAccessAllowedWithBlockListEntry) {
  // The block list takes priority over the allow list.
  SetAllowListEntry("https", "example.com", true);
  SetBlockListEntry("https", "example.com", false);

  EXPECT_FALSE(IsAllowed(https_example_origin()));
  EXPECT_TRUE(IsAllowed(https_sub_example_origin()));
}

TEST_F(OriginAccessListTest, IsAccessAllowedWildcardWithBlockListEntry) {
  SetAllowListEntry("https", "", true);
  AddBlockListEntry(
      "https", "google.com", false,
      network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority);

  EXPECT_TRUE(IsAllowed(https_example_origin()));
  EXPECT_FALSE(IsAllowed(https_google_origin()));
}

TEST_F(OriginAccessListTest, IsPriorityRespected) {
  SetAllowListEntry("https", "example.com", true);
  EXPECT_TRUE(IsAllowed(https_example_origin()));
  EXPECT_TRUE(IsAllowed(https_sub_example_origin()));

  // Higher priority blocklist overrides lower priority allowlist.
  AddBlockListEntry(
      "https", "example.com", true,
      network::mojom::CORSOriginAccessMatchPriority::kLowPriority);
  EXPECT_FALSE(IsAllowed(https_example_origin()));
  EXPECT_FALSE(IsAllowed(https_sub_example_origin()));

  // Higher priority allowlist overrides lower priority blocklist.
  AddAllowListEntry(
      "https", "example.com", false,
      network::mojom::CORSOriginAccessMatchPriority::kMediumPriority);
  EXPECT_TRUE(IsAllowed(https_example_origin()));
  EXPECT_FALSE(IsAllowed(https_sub_example_origin()));
}

TEST_F(OriginAccessListTest, IsPriorityRespectedReverse) {
  AddAllowListEntry(
      "https", "example.com", false,
      network::mojom::CORSOriginAccessMatchPriority::kMediumPriority);
  EXPECT_TRUE(IsAllowed(https_example_origin()));
  EXPECT_FALSE(IsAllowed(https_sub_example_origin()));

  AddBlockListEntry(
      "https", "example.com", true,
      network::mojom::CORSOriginAccessMatchPriority::kLowPriority);
  EXPECT_TRUE(IsAllowed(https_example_origin()));
  EXPECT_FALSE(IsAllowed(https_sub_example_origin()));

  AddAllowListEntry(
      "https", "example.com", true,
      network::mojom::CORSOriginAccessMatchPriority::kDefaultPriority);
  EXPECT_TRUE(IsAllowed(https_example_origin()));
  EXPECT_FALSE(IsAllowed(https_sub_example_origin()));
}

}  // namespace

}  // namespace cors

}  // namespace network
