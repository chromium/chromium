// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_WEB_WEBENGINE_BROWSER_FAKE_SEMANTIC_TREE_H_
#define FUCHSIA_WEB_WEBENGINE_BROWSER_FAKE_SEMANTIC_TREE_H_

#include <fuchsia/accessibility/semantics/cpp/fidl.h>
#include <fuchsia/accessibility/semantics/cpp/fidl_test_base.h>
#include <lib/fidl/cpp/binding.h>

#include <string_view>
#include <unordered_map>

#include "base/functional/callback.h"

class FakeSemanticTree
    : public fuchsia::accessibility::semantics::testing::SemanticTree_TestBase {
 public:
  FakeSemanticTree();
  ~FakeSemanticTree() override;

  FakeSemanticTree(const FakeSemanticTree&) = delete;
  FakeSemanticTree& operator=(const FakeSemanticTree&) = delete;

  // Binds |semantic_tree_request| to |this|.
  void Bind(
      fidl::InterfaceRequest<fuchsia::accessibility::semantics::SemanticTree>
          semantic_tree_request);

  // Checks that the tree is complete and that there are no dangling nodes by
  // traversing the tree starting at the root. Keeps track of how many nodes are
  // visited to make sure there aren't dangling nodes in |nodes_|.
  bool IsTreeValid(fuchsia::accessibility::semantics::Node* node,
                   size_t* tree_size);

  // Disconnects the SemanticTree binding.
  void Disconnect();

  void RunUntilNodeCountAtLeast(size_t count);
  void RunUntilNodeWithLabelIsInTree(std::string_view label);
  void RunUntilCommitCountIs(size_t count);
  void RunUntilConditionIsTrue(base::RepeatingCallback<bool()> condition);
  void SetNodeUpdatedCallback(uint32_t node_id,
                              base::OnceClosure node_updated_callback);
  fuchsia::accessibility::semantics::Node* GetNodeWithId(uint32_t id);

  // For the functions below, it is possible there are multiple nodes with the
  // same identifier.
  // Get the the first node in the document matching |label|, using a
  // depth-first search.
  fuchsia::accessibility::semantics::Node* GetNodeFromLabel(
      std::string_view label);
  fuchsia::accessibility::semantics::Node* GetNodeFromRole(
      fuchsia::accessibility::semantics::Role role);

  size_t tree_size() const { return nodes_.size(); }
  void Clear();

  size_t num_delete_calls() const { return num_delete_calls_; }
  size_t num_update_calls() const { return num_update_calls_; }
  size_t num_commit_calls() const { return num_commit_calls_; }

  // fuchsia::accessibility::semantics::SemanticTree implementation.
  void UpdateSemanticNodes(
      std::vector<fuchsia::accessibility::semantics::Node> nodes) final;
  void DeleteSemanticNodes(std::vector<uint32_t> node_ids) final;
  void CommitUpdates(CommitUpdatesCallback callback) final;

  void NotImplemented_(const std::string& name) final;

 private:
  // Get the first node matching |label|, in the subtree rooted at |node|, using
  // a depth first search.
  fuchsia::accessibility::semantics::Node* GetNodeFromLabelRecursive(
      fuchsia::accessibility::semantics::Node& node,
      std::string_view label);

  fidl::Binding<fuchsia::accessibility::semantics::SemanticTree>
      semantic_tree_binding_;
  std::unordered_map<uint32_t, fuchsia::accessibility::semantics::Node> nodes_;
  base::RepeatingClosure on_commit_updates_;

  uint32_t node_wait_id_;
  base::OnceClosure on_node_updated_callback_;

  size_t num_delete_calls_ = 0;
  size_t num_update_calls_ = 0;
  size_t num_commit_calls_ = 0;
};

#endif  // FUCHSIA_WEB_WEBENGINE_BROWSER_FAKE_SEMANTIC_TREE_H_
