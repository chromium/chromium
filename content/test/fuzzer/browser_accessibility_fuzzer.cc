// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuzzer/FuzzedDataProvider.h>

#include "base/at_exit.h"
#include "base/command_line.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/one_shot_accessibility_tree_search.h"
#include "content/browser/accessibility/test_browser_accessibility_delegate.h"

struct Env {
  Env() { base::CommandLine::Init(0, nullptr); }
  base::AtExitManager at_exit;
};

namespace content {

// Return an accessibility role to use in a tree to fuzz. Out of the
// dozens of roles, these are the ones that tend to have special meaning
// or otherwise affect the logic in BrowserAccessibility code.
ax::mojom::Role GetInterestingRole(FuzzedDataProvider& fdp) {
  switch (fdp.ConsumeIntegralInRange(0, 12)) {
    default:
    case 0:
      return ax::mojom::Role::kIgnored;
    case 1:
      return ax::mojom::Role::kStaticText;
    case 2:
      return ax::mojom::Role::kInlineTextBox;
    case 3:
      return ax::mojom::Role::kParagraph;
    case 4:
      return ax::mojom::Role::kLineBreak;
    case 5:
      return ax::mojom::Role::kGenericContainer;
    case 6:
      return ax::mojom::Role::kButton;
    case 7:
      return ax::mojom::Role::kTextField;
    case 8:
      return ax::mojom::Role::kIframePresentational;
    case 9:
      return ax::mojom::Role::kIframe;
    case 10:
      return ax::mojom::Role::kHeading;
    case 11:
      return ax::mojom::Role::kPopUpButton;
    case 12:
      return ax::mojom::Role::kLink;
  }
}

// Add some states to the node based on the FuzzedDataProvider.
// Currently we're messing with ignored and invisible because that
// affects a lot of the tree walking code.
void AddStates(FuzzedDataProvider& fdp, ui::AXNodeData* node) {
  if (fdp.ConsumeBool())
    node->AddState(ax::mojom::State::kIgnored);
  if (fdp.ConsumeBool())
    node->AddState(ax::mojom::State::kInvisible);
}

// Construct an accessibility tree. The shape of the tree is static, but
// some of the properties of the nodes in the tree are determined by
// the fuzz input. Once the tree is constructed, fuzz by calling some
// functions that walk the tree in various ways to ensure they don't crash.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  static Env env;
  FuzzedDataProvider fdp(data, size);

  // The tree structure is always the same, only the data changes.
  //
  //       1
  //    /      \
  //  2     3    4
  // / \   / \  / \
  // 5 6   7 8  9 10
  //
  // In addition, there's a child tree that may be linked off one of
  // the nodes.

  ui::AXTreeID parent_tree_id = ui::AXTreeID::CreateNewAXTreeID();
  ui::AXTreeID child_tree_id = ui::AXTreeID::CreateNewAXTreeID();

  const int num_nodes = 10;

  ui::AXTreeUpdate tree;
  tree.root_id = 1;
  tree.tree_data.tree_id = parent_tree_id;
  tree.has_tree_data = true;
  tree.nodes.resize(10);

  tree.nodes[0].id = 1;
  tree.nodes[0].role = ax::mojom::Role::kRootWebArea;
  tree.nodes[0].child_ids = {2, 3, 4};
  AddStates(fdp, &tree.nodes[0]);

  tree.nodes[1].id = 2;
  tree.nodes[1].role = GetInterestingRole(fdp);
  tree.nodes[1].child_ids = {5, 6};
  AddStates(fdp, &tree.nodes[1]);

  tree.nodes[2].id = 3;
  tree.nodes[2].role = GetInterestingRole(fdp);
  tree.nodes[2].child_ids = {7, 8};
  AddStates(fdp, &tree.nodes[2]);

  tree.nodes[3].id = 4;
  tree.nodes[3].role = GetInterestingRole(fdp);
  tree.nodes[3].child_ids = {9, 10};
  AddStates(fdp, &tree.nodes[3]);

  for (int i = 4; i < num_nodes; i++) {
    tree.nodes[i].id = i + 1;
    tree.nodes[i].role = GetInterestingRole(fdp);
    AddStates(fdp, &tree.nodes[i]);
  }

  for (int i = 0; i < num_nodes; i++)
    tree.nodes[i].SetName(fdp.ConsumeRandomLengthString(5));

  // Optionally, embed the child tree in the parent tree.
  int embedder_node = fdp.ConsumeIntegralInRange(0, num_nodes);
  if (embedder_node > 0)
    tree.nodes[embedder_node - 1].AddStringAttribute(
        ax::mojom::StringAttribute::kChildTreeId, child_tree_id.ToString());

  VLOG(1) << tree.ToString();

  // The child tree is trivial, just one node. That's still enough to exercise
  // a lot of paths.

  ui::AXTreeUpdate child_tree;
  child_tree.root_id = 1;
  child_tree.tree_data.tree_id = child_tree_id;
  child_tree.has_tree_data = true;
  child_tree.nodes.resize(1);

  child_tree.nodes[0].id = 1;
  child_tree.nodes[0].role = ax::mojom::Role::kRootWebArea;

  VLOG(1) << child_tree.ToString();

  TestBrowserAccessibilityDelegate delegate;
  std::unique_ptr<BrowserAccessibilityManager> manager(
      BrowserAccessibilityManager::Create(tree, &delegate));
  std::unique_ptr<BrowserAccessibilityManager> child_manager(
      BrowserAccessibilityManager::Create(child_tree, &delegate));

  // We want to call a bunch of functions but we don't care what the
  // return values are. To ensure the compiler doesn't optimize the calls
  // away, push the return values onto a vector and make an assertion about
  // it at the end.
  std::vector<void*> results;

  // Test some tree-walking functions.
  BrowserAccessibility* root = manager->GetRoot();
  results.push_back(root->PlatformDeepestFirstChild());
  results.push_back(root->PlatformDeepestLastChild());
  results.push_back(root->InternalDeepestFirstChild());
  results.push_back(root->InternalDeepestLastChild());

  // Test OneShotAccessibilityTreeSearch.
  OneShotAccessibilityTreeSearch search(manager->GetRoot());
  search.SetDirection(fdp.ConsumeBool()
                          ? OneShotAccessibilityTreeSearch::FORWARDS
                          : OneShotAccessibilityTreeSearch::BACKWARDS);
  search.SetImmediateDescendantsOnly(fdp.ConsumeBool());
  search.SetCanWrapToLastElement(fdp.ConsumeBool());
  search.SetOnscreenOnly(fdp.ConsumeBool());
  if (fdp.ConsumeBool())
    search.AddPredicate(AccessibilityButtonPredicate);
  if (fdp.ConsumeBool())
    search.SetSearchText(fdp.ConsumeRandomLengthString(5));
  size_t matches = search.CountMatches();
  for (size_t i = 0; i < matches; i++) {
    results.push_back(search.GetMatchAtIndex(i));
  }

  // This is just to ensure that none of the above code gets optimized away.
  CHECK_NE(0U, results.size());

  // Add a node, possibly clearing old children.
  int node_id = num_nodes + 1;
  int parent = fdp.ConsumeIntegralInRange(0, num_nodes);

  ui::AXTreeUpdate update;
  update.nodes.resize(2);
  update.nodes[0].id = parent;
  update.nodes[0].child_ids = {node_id};
  update.nodes[1].id = node_id;
  update.nodes[1].role = GetInterestingRole(fdp);
  AddStates(fdp, &update.nodes[1]);

  AXEventNotificationDetails notification;
  notification.updates.resize(1);
  notification.updates[0] = update;

  CHECK(manager->OnAccessibilityEvents(notification));

  return 0;
}

}  // namespace content
