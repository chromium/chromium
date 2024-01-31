// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_client_hints_controller_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"

using network::mojom::WebClientHintsType::kUAArch;
using network::mojom::WebClientHintsType::kUAModel;
using network::mojom::WebClientHintsType::kUAPlatform;

namespace headless {

TEST(HeadlessClientHintsControllerDelegateTests, GetNetworkQualityTracker) {
  HeadlessClientHintsControllerDelegate delegate;
  EXPECT_EQ(delegate.GetNetworkQualityTracker(), nullptr);
}

TEST(HeadlessClientHintsControllerDelegateTests, AreThirdPartyCookiesBlocked) {
  HeadlessClientHintsControllerDelegate delegate;
  EXPECT_FALSE(delegate.AreThirdPartyCookiesBlocked(GURL("https://example.com"),
                                                    nullptr));
}

TEST(HeadlessClientHintsControllerDelegateTests, PersistentClientHints) {
  HeadlessClientHintsControllerDelegate delegate;
  auto kOrigin = url::Origin::Create(GURL("https://example.com"));
  delegate.PersistClientHints(kOrigin, nullptr, {kUAArch, kUAPlatform});

  blink::EnabledClientHints enabled_client_hints;
  delegate.GetAllowedClientHintsFromSource(kOrigin, &enabled_client_hints);
  EXPECT_EQ(enabled_client_hints.GetEnabledHints(),
            std::vector({kUAArch, kUAPlatform}));

  delegate.PersistClientHints(kOrigin, nullptr, {kUAPlatform, kUAModel});
  delegate.GetAllowedClientHintsFromSource(kOrigin, &enabled_client_hints);
  EXPECT_EQ(enabled_client_hints.GetEnabledHints(),
            std::vector({kUAPlatform, kUAModel}));
}

TEST(HeadlessClientHintsControllerDelegateTests, PersistentClientHintsNotSet) {
  HeadlessClientHintsControllerDelegate delegate;
  auto kOrigin = url::Origin::Create(GURL("https://example.com"));

  blink::EnabledClientHints enabled_client_hints;
  delegate.GetAllowedClientHintsFromSource(kOrigin, &enabled_client_hints);
  EXPECT_TRUE(enabled_client_hints.GetEnabledHints().empty());
}

TEST(HeadlessClientHintsControllerDelegateTests,
     PersistentClientHintsEmptyOrigin) {
  HeadlessClientHintsControllerDelegate delegate;
  auto kOrigin = url::Origin();
  delegate.PersistClientHints(kOrigin, nullptr, {kUAArch, kUAPlatform});

  // Hints are not persisted for empty origins.
  blink::EnabledClientHints enabled_client_hints;
  delegate.GetAllowedClientHintsFromSource(kOrigin, &enabled_client_hints);
  EXPECT_TRUE(enabled_client_hints.GetEnabledHints().empty());
}

TEST(HeadlessClientHintsControllerDelegateTests,
     PersistentClientHintsInsecureOrigin) {
  HeadlessClientHintsControllerDelegate delegate;
  auto kOrigin = url::Origin::Create(GURL("http://example.com"));
  delegate.PersistClientHints(kOrigin, nullptr, {kUAArch, kUAPlatform});

  // Hints are not persisted for insecure origins.
  blink::EnabledClientHints enabled_client_hints;
  delegate.GetAllowedClientHintsFromSource(kOrigin, &enabled_client_hints);
  EXPECT_TRUE(enabled_client_hints.GetEnabledHints().empty());
}

TEST(HeadlessClientHintsControllerDelegateTests,
     PersistentClientHintsDifferentOrigins) {
  HeadlessClientHintsControllerDelegate delegate;
  auto kOrigin1 = url::Origin::Create(GURL("https://one.example.com"));
  auto kOrigin2 = url::Origin::Create(GURL("https://two.example.com"));
  delegate.PersistClientHints(kOrigin1, nullptr, {kUAArch, kUAPlatform});
  delegate.PersistClientHints(kOrigin2, nullptr, {kUAPlatform, kUAModel});

  blink::EnabledClientHints enabled_client_hints;
  delegate.GetAllowedClientHintsFromSource(kOrigin1, &enabled_client_hints);
  EXPECT_EQ(enabled_client_hints.GetEnabledHints(),
            std::vector({kUAArch, kUAPlatform}));

  delegate.GetAllowedClientHintsFromSource(kOrigin2, &enabled_client_hints);
  EXPECT_EQ(enabled_client_hints.GetEnabledHints(),
            std::vector({kUAPlatform, kUAModel}));
}

TEST(HeadlessClientHintsControllerDelegateTests,
     PersistentClientHintsAdditional) {
  HeadlessClientHintsControllerDelegate delegate;
  auto kOrigin = url::Origin::Create(GURL("https://example.com"));
  delegate.PersistClientHints(kOrigin, nullptr, {kUAArch, kUAPlatform});
  delegate.SetAdditionalClientHints({kUAPlatform, kUAModel});

  blink::EnabledClientHints enabled_client_hints;
  delegate.GetAllowedClientHintsFromSource(kOrigin, &enabled_client_hints);
  EXPECT_EQ(enabled_client_hints.GetEnabledHints(),
            std::vector({kUAArch, kUAPlatform, kUAModel}));

  delegate.ClearAdditionalClientHints();

  delegate.GetAllowedClientHintsFromSource(kOrigin, &enabled_client_hints);
  EXPECT_EQ(enabled_client_hints.GetEnabledHints(),
            std::vector({kUAArch, kUAPlatform}));
}

TEST(HeadlessClientHintsControllerDelegateTests, IsJavaScriptAllowed) {
  HeadlessClientHintsControllerDelegate delegate;
  EXPECT_TRUE(delegate.IsJavaScriptAllowed(GURL(), nullptr));
}

TEST(HeadlessClientHintsControllerDelegateTests, GetUserAgentMetadata) {
  HeadlessClientHintsControllerDelegate delegate;
  EXPECT_EQ(delegate.GetUserAgentMetadata(),
            HeadlessBrowser::GetUserAgentMetadata());
}

TEST(HeadlessClientHintsControllerDelegateTests,
     MostRecentMainFrameViewportSize) {
  HeadlessClientHintsControllerDelegate delegate;
  EXPECT_EQ(delegate.GetMostRecentMainFrameViewportSize(), gfx::Size(800, 600));
  delegate.SetMostRecentMainFrameViewportSize(gfx::Size(640, 480));
  EXPECT_EQ(delegate.GetMostRecentMainFrameViewportSize(), gfx::Size(640, 480));
}

}  // namespace headless
