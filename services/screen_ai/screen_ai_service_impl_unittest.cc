// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/screen_ai/screen_ai_service_impl.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_tree.h"

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

}  // namespace screen_ai
