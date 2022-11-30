// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/cors_origin_pattern_setter.h"

#include <string>
#include <vector>

#include "base/run_loop.h"
#include "content/public/browser/shared_cors_origin_access_list.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_browser_context.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "services/network/public/cpp/cors/origin_access_list.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

network::mojom::CorsOriginPatternPtr CreateTestPattern(
    std::string target_domain) {
  std::string scheme = "chrome-extension";
  uint16_t port = 123;
  network::mojom::CorsDomainMatchMode domain_mode =
      network::mojom::CorsDomainMatchMode::kAllowSubdomains;
  network::mojom::CorsPortMatchMode port_mode =
      network::mojom::CorsPortMatchMode::kAllowOnlySpecifiedPort;
  network::mojom::CorsOriginAccessMatchPriority priority =
      network::mojom::CorsOriginAccessMatchPriority::kDefaultPriority;

  return network::mojom::CorsOriginPattern::New(
      scheme, target_domain, port, domain_mode, port_mode, priority);
}

}  // namespace

TEST(CorsOriginPatternSetter, InitialEmptyList) {
  content::BrowserTaskEnvironment task_environment;
  scoped_refptr<content::SharedCorsOriginAccessList> shared_list;

  {
    content::TestBrowserContext browser_context;
    shared_list = browser_context.GetSharedCorsOriginAccessList();
    ASSERT_TRUE(shared_list);

    // The list is empty (in absence of calls to CorsOriginPatternSetter::Set).
    std::vector<network::mojom::CorsOriginAccessPatternsPtr> patterns =
        shared_list->GetOriginAccessList().CreateCorsOriginAccessPatternsList();
    EXPECT_EQ(0u, patterns.size());

    // |shared_list| should own 1 ref-count + |browser_context|'s UserData
    // should own 1 ref-count.
    EXPECT_FALSE(shared_list->HasOneRef());
  }

  // |shared_list| should own the only remaining ref-count (i.e. the product
  // code shouldn't hold any ref-counts at this point).
  EXPECT_TRUE(shared_list->HasOneRef());
}

TEST(CorsOriginPatternSetter, ClonePatterns) {
  content::BrowserTaskEnvironment task_environment;
  const char kTestDomain[] = "example.com";
  network::mojom::CorsOriginPatternPtr test_pattern =
      CreateTestPattern(kTestDomain);

  std::vector<network::mojom::CorsOriginPatternPtr> original_patterns;
  original_patterns.push_back(CreateTestPattern(kTestDomain));
  EXPECT_EQ(1u, original_patterns.size());
  EXPECT_EQ(*test_pattern, *original_patterns[0]);

  std::vector<network::mojom::CorsOriginPatternPtr> cloned_patterns =
      mojo::Clone(original_patterns);
  // `original_patterns` is not affected by the Clone method.
  EXPECT_EQ(1u, original_patterns.size());
  EXPECT_EQ(*test_pattern, *original_patterns[0]);
  // Equivalent contents, but different pointers (i.e. separate objects).
  EXPECT_EQ(1u, cloned_patterns.size());
  EXPECT_EQ(*original_patterns[0], *cloned_patterns[0]);
  EXPECT_NE(original_patterns[0].get(), cloned_patterns[0].get());
}

TEST(CorsOriginPatternSetter, Set) {
  content::BrowserTaskEnvironment task_environment;
  scoped_refptr<content::SharedCorsOriginAccessList> shared_list;

  {
    content::TestBrowserContext browser_context;

    const char kTestDomain[] = "example.com";
    network::mojom::CorsOriginPatternPtr test_pattern =
        CreateTestPattern(kTestDomain);
    url::Origin source_origin =
        url::Origin::Create(GURL("https://initiator.com"));
    std::vector<network::mojom::CorsOriginPatternPtr> allowlist;
    allowlist.push_back(CreateTestPattern(kTestDomain));

    // Call CorsOriginPatternSetter::Set and wait until it completes.
    base::RunLoop run_loop;
    CorsOriginPatternSetter::Set(&browser_context, source_origin,
                                 std::move(allowlist), {},
                                 run_loop.QuitClosure());
    run_loop.Run();

    // Verify that the results got properly stored.
    shared_list = browser_context.GetSharedCorsOriginAccessList();
    ASSERT_TRUE(shared_list);
    std::vector<network::mojom::CorsOriginAccessPatternsPtr> patterns =
        shared_list->GetOriginAccessList().CreateCorsOriginAccessPatternsList();
    ASSERT_EQ(1u, patterns.size());
    ASSERT_TRUE(patterns[0]);
    EXPECT_EQ(source_origin, patterns[0]->source_origin);
    EXPECT_EQ(0u, patterns[0]->block_patterns.size());
    ASSERT_EQ(1u, patterns[0]->allow_patterns.size());
    EXPECT_EQ(*test_pattern, *patterns[0]->allow_patterns[0]);

    // TODO(lukasza): Add verification that
    // network::mojom::NetworkContext::SetCorsOriginAccessListsForOrigin got
    // called.  The immediate problem is that TestBrowserContext has 0
    // StoragePartition objects - we would need to find a way to add
    // TestStoragePartition objects with test-controlled TestNetworkContext.

    // |shared_list| should own 1 ref-count + |browser_context|'s UserData
    // should own 1 ref-count.
    EXPECT_FALSE(shared_list->HasOneRef());
  }

  // |shared_list| should own the only remaining ref-count (i.e. the product
  // code shouldn't hold any ref-counts at this point).
  EXPECT_TRUE(shared_list->HasOneRef());
}

}  // namespace content
