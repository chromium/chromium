// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/content_browser_client.h"

#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "content/public/common/content_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/storage_key/storage_key.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

TEST(ContentBrowserClientTest, IsFullCookieAccessAllowed) {
  ContentBrowserClient client;
  const GURL url = GURL("https://example.com");
  const url::Origin origin = url::Origin::Create(url);

  // Without a nonce, full cookie access is allowed by default.
  EXPECT_TRUE(client.IsFullCookieAccessAllowed(
      nullptr, nullptr, url, blink::StorageKey::CreateFirstParty(origin), {}));

  // With a nonce, full cookie access is not allowed.
  EXPECT_FALSE(client.IsFullCookieAccessAllowed(
      nullptr, nullptr, url,
      blink::StorageKey::CreateWithNonce(origin,
                                         base::UnguessableToken::Create()),
      {}));
}

#if BUILDFLAG(IS_ANDROID)
TEST(ContentBrowserClientTest, BlocksContentUriNavigationByDefault) {
  ContentBrowserClient client;
  EXPECT_FALSE(client.IsContentSchemeURLNavigationAllowed(
      GURL("content://media/external/downloads/1")));
}

TEST(ContentBrowserClientTest,
     AllowsContentUriNavigationWhenDefaultBlockingDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      features::kBlockContentUriNavigationByDefault);
  ContentBrowserClient client;
  EXPECT_TRUE(client.IsContentSchemeURLNavigationAllowed(
      GURL("content://media/external/downloads/1")));
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace content
