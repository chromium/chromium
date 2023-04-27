// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/browsing_context_group_swap.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

TEST(BrowsingContextGroupSwap, CreateDefault) {
  BrowsingContextGroupSwap default_swap =
      BrowsingContextGroupSwap::CreateDefault();

  EXPECT_EQ(BrowsingContextGroupSwapType::kNoSwap, default_swap.type());
  EXPECT_FALSE(default_swap.ShouldSwap());
  EXPECT_FALSE(default_swap.ShouldClearProxiesOnCommit());
  EXPECT_FALSE(default_swap.ShouldClearWindowName());
}

TEST(BrowsingContextGroupSwap, CreateNoSwap) {
  BrowsingContextGroupSwap no_swap = BrowsingContextGroupSwap::CreateNoSwap(
      ShouldSwapBrowsingInstance::kNo_NotMainFrame);

  EXPECT_EQ(BrowsingContextGroupSwapType::kNoSwap, no_swap.type());
  EXPECT_FALSE(no_swap.ShouldSwap());
  EXPECT_FALSE(no_swap.ShouldClearProxiesOnCommit());
  EXPECT_FALSE(no_swap.ShouldClearWindowName());
  EXPECT_EQ(ShouldSwapBrowsingInstance::kNo_NotMainFrame, no_swap.reason());
}

TEST(BrowsingContextGroupSwap, CreateCoopSwap) {
  BrowsingContextGroupSwap coop_swap =
      BrowsingContextGroupSwap::CreateCoopSwap();

  EXPECT_EQ(BrowsingContextGroupSwapType::kCoopSwap, coop_swap.type());
  EXPECT_TRUE(coop_swap.ShouldSwap());
  EXPECT_TRUE(coop_swap.ShouldClearProxiesOnCommit());
  EXPECT_TRUE(coop_swap.ShouldClearWindowName());
  EXPECT_EQ(ShouldSwapBrowsingInstance::kYes_ForceSwap, coop_swap.reason());
}

TEST(BrowsingContextGroupSwap, CreateRelatedCoopSwap) {
  BrowsingContextGroupSwap related_coop_swap =
      BrowsingContextGroupSwap::CreateRelatedCoopSwap();

  EXPECT_EQ(BrowsingContextGroupSwapType::kRelatedCoopSwap,
            related_coop_swap.type());
  EXPECT_TRUE(related_coop_swap.ShouldSwap());
  EXPECT_FALSE(related_coop_swap.ShouldClearProxiesOnCommit());
  EXPECT_TRUE(related_coop_swap.ShouldClearWindowName());
  EXPECT_EQ(ShouldSwapBrowsingInstance::kYes_ForceSwap,
            related_coop_swap.reason());
}

TEST(BrowsingContextGroupSwap, CreateSecuritySwap) {
  BrowsingContextGroupSwap security_swap =
      BrowsingContextGroupSwap::CreateSecuritySwap();

  EXPECT_EQ(BrowsingContextGroupSwapType::kSecuritySwap, security_swap.type());
  EXPECT_TRUE(security_swap.ShouldSwap());
  EXPECT_FALSE(security_swap.ShouldClearProxiesOnCommit());
  EXPECT_FALSE(security_swap.ShouldClearWindowName());
  EXPECT_EQ(ShouldSwapBrowsingInstance::kYes_ForceSwap, security_swap.reason());
}

TEST(BrowsingContextGroupSwap, CreateProactiveSwap) {
  BrowsingContextGroupSwap proactive_swap =
      BrowsingContextGroupSwap::CreateProactiveSwap(
          ShouldSwapBrowsingInstance::kYes_SameSiteProactiveSwap);

  EXPECT_EQ(BrowsingContextGroupSwapType::kProactiveSwap,
            proactive_swap.type());
  EXPECT_TRUE(proactive_swap.ShouldSwap());
  EXPECT_FALSE(proactive_swap.ShouldClearProxiesOnCommit());
  EXPECT_FALSE(proactive_swap.ShouldClearWindowName());
  EXPECT_EQ(ShouldSwapBrowsingInstance::kYes_SameSiteProactiveSwap,
            proactive_swap.reason());
}

}  // namespace

}  // namespace content
