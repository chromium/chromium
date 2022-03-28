// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/speculation_rules/prefetch/prefetch_container.h"
#include "content/browser/speculation_rules/prefetch/prefetch_status.h"
#include "content/browser/speculation_rules/prefetch/prefetch_type.h"
#include "content/public/browser/global_routing_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

class PrefetchContainerTest : public ::testing::Test {};

TEST_F(PrefetchContainerTest, CreatePrefetchContainer) {
  PrefetchContainer prefetch_container(
      GlobalRenderFrameHostId(1234, 5678), GURL("https://test.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true));

  EXPECT_EQ(prefetch_container.GetReferringRenderFrameHostId(),
            GlobalRenderFrameHostId(1234, 5678));
  EXPECT_EQ(prefetch_container.GetURL(), GURL("https://test.com"));
  EXPECT_EQ(prefetch_container.GetPrefetchType(),
            PrefetchType(/*use_isolated_network_context=*/true,
                         /*use_prefetch_proxy=*/true));

  EXPECT_EQ(prefetch_container.GetPrefetchContainerKey(),
            std::make_pair(GlobalRenderFrameHostId(1234, 5678),
                           GURL("https://test.com")));
}

TEST_F(PrefetchContainerTest, PrefetchStatus) {
  PrefetchContainer prefetch_container(
      GlobalRenderFrameHostId(1234, 5678), GURL("https://test.com"),
      PrefetchType(/*use_isolated_network_context=*/true,
                   /*use_prefetch_proxy=*/true));

  EXPECT_FALSE(prefetch_container.HasPrefetchStatus());

  prefetch_container.SetPrefetchStatus(PrefetchStatus::kPrefetchNotStarted);

  EXPECT_TRUE(prefetch_container.HasPrefetchStatus());
  EXPECT_EQ(prefetch_container.GetPrefetchStatus(),
            PrefetchStatus::kPrefetchNotStarted);
}

}  // namespace
}  // namespace content
