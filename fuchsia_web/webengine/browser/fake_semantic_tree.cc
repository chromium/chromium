// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia_web/webengine/browser/fake_semantic_tree.h"

#include <lib/fidl/cpp/binding.h>
#include <zircon/types.h>

#include <string_view>

#include "base/auto_reset.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/platform/fuchsia/semantic_provider.h"

FakeSemanticTree::FakeSemanticTree() : semantic_tree_binding_(this) {}
FakeSemanticTree::~FakeSemanticTree() = default;

void FakeSemanticTree::Bind(
    fidl::InterfaceRequest<fuchsia::accessibility::semantics::SemanticTree>
        semantic_tree_request) {
  semantic_tree_binding_.Bind(std::move(semantic_tree_request));
}

bool FakeSemanticTree::IsTreeValid(
    fuchsia::accessibility::semantics::Node* node,
    size_t* tree_size) {
  (*tree_size)++;

  if (!node->has_child_ids())
    return true;

  bool is_valid = true;
  for (auto c : node->child_ids()) {
    fuchsia::accessibility::semantics::Node* child = GetNodeWithId(c);
    if (!child)
      return false;

    is_valid &= IsTreeValid(child, tree_size);
  }
  return is_valid;
}

void FakeSemanticTree::Disconnect() {
  semantic_tree_binding_.Close(ZX_ERR_INTERNAL);
}

void FakeSemanticTree::RunUntilNodeCountAtLeast(size_t count) {
  DCHECK(!on_commit_updates_);
  if (nodes_.size() >= count)
    return;

  base::RunLoop run_loop;
  base::AutoReset<base::RepeatingClosure> auto_reset(
      &on_commit_updates_,
      base::BindLambdaForTesting([this, count, &run_loop]() {
        if (nodes_.size() >= count) {
          run_loop.Quit();
        }
      }));
  run_loop.Run();
}

void FakeSemanticTree::RunUntilNodeWithLabelIsInTree(std::string_view label) {
  DCHECK(!on_commit_updates_);
  if (GetNodeFromLabel(label))
    return;

  base::RunLoop run_loop;
  base::AutoReset<base::RepeatingClosure> auto_reset(
      &on_commit_updates_,
      base::BindLambdaForTesting([this, label, &run_loop]() {
        if (GetNodeFromLabel(label))
          run_loop.Quit();
      }));
  run_loop.Run();
}

void FakeSemanticTree::RunUntilCommitCountIs(size_t count) {
  DCHECK(!on_commit_updates_);
  if (count == num_commit_calls_)
    return;

  base::RunLoop run_loop;
  base::AutoReset<base::RepeatingClosure> auto_reset(
      &on_commit_updates_,
      base::BindLambdaForTesting([this, count, &run_loop]() {
        if (static_cast<size_t>(num_commit_calls_) == count) {
          run_loop.Quit();
        }
      }));
  run_loop.Run();
}

void FakeSemanticTree::SetNodeUpdatedCallback(
    uint32_t node_id,
    base::OnceClosure node_updated_callback) {
  node_wait_id_ = node_id;
  on_node_updated_callback_ = std::move(node_updated_callback);
}

fuchsia::accessibility::semantics::Node* FakeSemanticTree::GetNodeWithId(
    uint32_t id) {
  auto it = nodes_.find(id);
  return it == nodes_.end() ? nullptr : &it->second;
}

fuchsia::accessibility::semantics::Node* FakeSemanticTree::GetNodeFromLabel(
    std::string_view label) {
  auto it = nodes_.find(ui::AXFuchsiaSemanticProvider::kFuchsiaRootNodeId);
  if (it == nodes_.end()) {
    DCHECK(nodes_.empty()) << "Missing root.";
    return nullptr;
  }

  fuchsia::accessibility::semantics::Node& root = it->second;

  return GetNodeFromLabelRecursive(root, label);
}

fuchsia::accessibility::semantics::Node*
FakeSemanticTree::GetNodeFromLabelRecursive(
    fuchsia::accessibility::semantics::Node& node,
    std::string_view label) {
  if (node.has_attributes() && node.attributes().has_label() &&
      node.attributes().label() == label) {
    return &node;
  }

  if (!node.has_child_ids())
    return nullptr;

  for (auto child_id : node.child_ids()) {
    fuchsia::accessibility::semantics::Node* child = GetNodeWithId(child_id);
    if (!child)
      return nullptr;

    fuchsia::accessibility::semantics::Node* matching_node =
        GetNodeFromLabelRecursive(*child, label);
    if (matching_node)
      return matching_node;
  }

  return nullptr;
}

fuchsia::accessibility::semantics::Node* FakeSemanticTree::GetNodeFromRole(
    fuchsia::accessibility::semantics::Role role) {
  for (auto& n : nodes_) {
    auto* node = &n.second;
    if (node->has_role() && node->role() == role)
      return node;
  }

  return nullptr;
}

void FakeSemanticTree::UpdateSemanticNodes(
    std::vector<fuchsia::accessibility::semantics::Node> nodes) {
  num_update_calls_++;
  bool wait_node_updated = false;
  for (auto& node : nodes) {
    if (node.node_id() == node_wait_id_ && on_node_updated_callback_)
      wait_node_updated = true;

    nodes_[node.node_id()] = std::move(node);
  }

  if (wait_node_updated)
    std::move(on_node_updated_callback_).Run();
}

void FakeSemanticTree::DeleteSemanticNodes(std::vector<uint32_t> node_ids) {
  num_delete_calls_++;
  for (auto id : node_ids)
    nodes_.erase(id);
}

void FakeSemanticTree::CommitUpdates(CommitUpdatesCallback callback) {
  num_commit_calls_++;
  callback();
  if (on_commit_updates_)
    on_commit_updates_.Run();
  if (nodes_.size() > 0) {
    size_t tree_size = 0;
    EXPECT_TRUE(IsTreeValid(GetNodeWithId(0), &tree_size));
    EXPECT_EQ(tree_size, nodes_.size());
  }
}

void FakeSemanticTree::NotImplemented_(const std::string& name) {
  NOTIMPLEMENTED() << name;
}

void FakeSemanticTree::Clear() {
  nodes_.clear();
}

void FakeSemanticTree::RunUntilConditionIsTrue(
    base::RepeatingCallback<bool()> condition) {
  DCHECK(!on_commit_updates_);
  if (condition.Run())
    return;

  base::RunLoop run_loop;
  base::AutoReset<base::RepeatingClosure> auto_reset(
      &on_commit_updates_,
      base::BindLambdaForTesting([&condition, &run_loop]() {
        if (condition.Run())
          run_loop.Quit();
      }));
  run_loop.Run();
}
