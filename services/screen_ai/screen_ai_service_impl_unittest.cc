// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/screen_ai/screen_ai_service_impl.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_tree.h"

#if BUILDFLAG(IS_LINUX)
#include "services/screen_ai/public/cpp/utilities.h"  // nogncheck
#endif

namespace screen_ai {

TEST(ScreenAIServiceImplTest, ComputeMainNode) {
  ui::AXTreeUpdate snapshot;
  ui::AXNodeData root;
  root.id = 1;
  ui::AXNodeData node1;
  node1.id = 2;
  ui::AXNodeData node2;
  node2.id = 3;
  ui::AXNodeData node3;
  node3.id = 4;
  ui::AXNodeData node4;
  node4.id = 5;
  ui::AXNodeData node5;
  node5.id = 6;
  root.child_ids = {node1.id, node2.id};
  node2.child_ids = {node3.id, node4.id, node5.id};
  snapshot.root_id = root.id;
  snapshot.nodes = {root, node1, node2, node3, node4, node5};

  ui::AXTree tree(snapshot);
  EXPECT_EQ(node2.id, ScreenAIService::ComputeMainNodeForTesting(
                          &tree, {node3.id, node4.id}));
  EXPECT_EQ(node2.id, ScreenAIService::ComputeMainNodeForTesting(
                          &tree, {node3.id, node4.id, node5.id}));
  EXPECT_EQ(node2.id, ScreenAIService::ComputeMainNodeForTesting(
                          &tree, {node3.id, node5.id}));
  EXPECT_EQ(root.id, ScreenAIService::ComputeMainNodeForTesting(
                         &tree, {node1.id, node2.id}));
  EXPECT_EQ(root.id,
            ScreenAIService::ComputeMainNodeForTesting(
                &tree, {node1.id, node2.id, node3.id, node4.id, node5.id}));
}

#if BUILDFLAG(IS_LINUX)
TEST(ScreenAIServiceImplTest, IsVulnerableToTlsDtvCrash) {
  // `dlinfo(handle, RTLD_DI_TLS_DATA, &tls_block)` returns a pointer to the TLS
  // block if Static TLS was allocated, or nullptr if pool was exhausted.
  // The vulnerability check only verifies whether `tls_block != nullptr` and
  // never dereferences the address. Any non-null stack variable address (such
  // as
  // `&mock_tls_block`) is a valid and safe mock representation of a non-null
  // TLS block allocation.
  int mock_tls_block = 42;

  // Safe: glibc >= 2.35 regardless of tls_block.
  EXPECT_FALSE(IsVulnerableToTlsDtvCrash_ForTesting("2.35", nullptr));
  EXPECT_FALSE(IsVulnerableToTlsDtvCrash_ForTesting("2.35", &mock_tls_block));
  EXPECT_FALSE(IsVulnerableToTlsDtvCrash_ForTesting("2.39", nullptr));
  EXPECT_FALSE(IsVulnerableToTlsDtvCrash_ForTesting("3.0", nullptr));

  // Safe: glibc < 2.35, but Static TLS allocated successfully (valid mock
  // address).
  EXPECT_FALSE(IsVulnerableToTlsDtvCrash_ForTesting("2.31", &mock_tls_block));
  EXPECT_FALSE(IsVulnerableToTlsDtvCrash_ForTesting("2.27", &mock_tls_block));

  // Vulnerable: glibc < 2.35 AND Static TLS exhausted (tls_block is nullptr).
  EXPECT_TRUE(IsVulnerableToTlsDtvCrash_ForTesting("2.31", nullptr));
  EXPECT_TRUE(IsVulnerableToTlsDtvCrash_ForTesting("2.27", nullptr));

  // Handle is null: should return false.
  EXPECT_FALSE(IsVulnerableToTlsDtvCrash(nullptr));
}
#endif  // BUILDFLAG(IS_LINUX)

}  // namespace screen_ai
