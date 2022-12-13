// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/back_forward_cache_impl.h"
#include "content/test/test_render_view_host.h"

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

}  // namespace content