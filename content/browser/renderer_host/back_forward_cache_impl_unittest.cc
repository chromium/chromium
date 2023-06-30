// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/back_forward_cache_impl.h"

#include "base/test/scoped_feature_list.h"
#include "content/test/test_render_frame_host.h"
#include "content/test/test_render_view_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/scheduler/web_scheduler_tracked_feature.h"

namespace content {

class BackForwardCacheImplTest : public RenderViewHostImplTestHarness {
 public:
  BackForwardCacheImplTest() = default;

  std::unique_ptr<BackForwardCacheCanStoreTreeResult> SetUpTree() {
    //     (a-1)
    //     /   |
    //  (b-1) (a-2)
    //    |    |
    //  (b-2) (b-3)
    auto tree_a_1 = CreateSameOriginTree();
    auto tree_a_2 = CreateSameOriginTree();
    auto tree_b_1 = CreateCrossOriginTree();
    auto tree_b_2 = CreateCrossOriginTree();
    auto tree_b_3 = CreateCrossOriginTree();
    tree_b_1->AppendChild(std::move(tree_b_2));
    tree_a_2->AppendChild(std::move(tree_b_3));
    tree_a_1->AppendChild(std::move(tree_b_1));
    tree_a_1->AppendChild(std::move(tree_a_2));
    return tree_a_1;
  }

 private:
  std::unique_ptr<BackForwardCacheCanStoreTreeResult> CreateSameOriginTree() {
    std::unique_ptr<BackForwardCacheCanStoreTreeResult> tree(
        new BackForwardCacheCanStoreTreeResult(/*is_same_origin=*/true,
                                               GURL("https://a.com/test")));
    return tree;
  }

  std::unique_ptr<BackForwardCacheCanStoreTreeResult> CreateCrossOriginTree() {
    BackForwardCacheCanStoreDocumentResult result;
    std::unique_ptr<BackForwardCacheCanStoreTreeResult> tree(
        new BackForwardCacheCanStoreTreeResult(/*is_same_origin=*/false,
                                               GURL("https://b.com/test")));
    return tree;
  }
};

TEST_F(BackForwardCacheImplTest, CrossOriginReachableFrameCount) {
  auto tree_root = SetUpTree();
  // The reachable cross-origin frames are b-1 and b-3.
  EXPECT_EQ(static_cast<int>(tree_root->GetCrossOriginReachableFrameCount()),
            2);
}

TEST_F(BackForwardCacheImplTest, FirstCrossOriginReachable) {
  auto tree_root = SetUpTree();
  int index = 0;
  // First cross-origin reachable frame (b-1) should be unmasked.
  auto result = tree_root->GetWebExposedNotRestoredReasonsInternal(index);
  // b-1 is unmasked.
  EXPECT_EQ(result->same_origin_details->children[0]->blocked,
            blink::mojom::BFCacheBlocked::kNo);
  // b-3 is masked.
  EXPECT_EQ(result->same_origin_details->children[1]
                ->same_origin_details->children[0]
                ->blocked,
            blink::mojom::BFCacheBlocked::kMasked);
}

TEST_F(BackForwardCacheImplTest, SecondCrossOriginReachable) {
  auto tree_root = SetUpTree();
  int index = 1;
  // Second cross-origin reachable frame (b-3) should be unmasked.
  auto result = tree_root->GetWebExposedNotRestoredReasonsInternal(index);
  // b-1 is unmasked.
  EXPECT_EQ(result->same_origin_details->children[0]->blocked,
            blink::mojom::BFCacheBlocked::kMasked);
  // b-3 is masked.
  EXPECT_EQ(result->same_origin_details->children[1]
                ->same_origin_details->children[0]
                ->blocked,
            blink::mojom::BFCacheBlocked::kNo);
}

// Covers BackForwardCache's cache size-related values used in Stable.
// See docs/back_forward_cache_size.md for more details.
class BackForwardCacheActiveSizeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{features::kBackForwardCache,
          {{"cache_size", "6"}, {"foreground_cache_size", "2"}}}},
        /*disabled_features=*/
        // Allow BackForwardCache for all devices regardless of their memory.
        {{features::kBackForwardCacheMemoryControls}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BackForwardCacheActiveSizeTest, ActiveCacheSize) {
  // The default cache sizes specified by kBackForwardCacheSize takes precedence
  // over kBackForwardCache.
  EXPECT_EQ(BackForwardCacheImpl::GetCacheSize(), 6u);
  EXPECT_EQ(BackForwardCacheImpl::GetForegroundedEntriesCacheSize(), 0u);
  EXPECT_FALSE(BackForwardCacheImpl::UsingForegroundBackgroundCacheSizeLimit());
}

// Covers overwriting BackForwardCache's cache size-related values.
// When "cache_size" or "foreground_cache_size" presents in both
// `kBackForwardCacheSize` and `features::kBackForwardCache`, the former should
// take precedence.
class BackForwardCacheOverwriteSizeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{kBackForwardCacheSize,
          {{"cache_size", "8"}, {"foreground_cache_size", "4"}}},
         {features::kBackForwardCache,
          {{"cache_size", "6"}, {"foreground_cache_size", "2"}}}},
        /*disabled_features=*/
        // Allow BackForwardCache for all devices regardless of their memory.
        {{features::kBackForwardCacheMemoryControls}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BackForwardCacheOverwriteSizeTest, OverwrittenCacheSize) {
  EXPECT_EQ(BackForwardCacheImpl::GetCacheSize(), 8u);
  EXPECT_EQ(BackForwardCacheImpl::GetForegroundedEntriesCacheSize(), 4u);
  EXPECT_TRUE(BackForwardCacheImpl::UsingForegroundBackgroundCacheSizeLimit());
}

// Covers BackForwardCache's default cache size-related values.
// Note that these tests don't cover the values configured from Finch.
class BackForwardCacheDefaultSizeTest : public ::testing::Test {
 protected:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        // Ensure BackForwardCache is enabled.
        {{features::kBackForwardCache, {}}},
        /*disabled_features=*/
        // Allow BackForwardCache for all devices regardless of their memory.
        {{features::kBackForwardCacheMemoryControls}});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(BackForwardCacheDefaultSizeTest, DefaultCacheSize) {
  // Default cache sizes are specified by kBackForwardCacheSize.
  EXPECT_EQ(BackForwardCacheImpl::GetCacheSize(), 6u);
  EXPECT_EQ(BackForwardCacheImpl::GetForegroundedEntriesCacheSize(), 0u);
  EXPECT_FALSE(BackForwardCacheImpl::UsingForegroundBackgroundCacheSizeLimit());
}

}  // namespace content
