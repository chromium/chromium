// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/ax_tree_distiller.h"

#include <memory>
#include <queue>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "content/renderer/accessibility/ax_tree_snapshotter_impl.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"

namespace {

static const ax::mojom::Role kRolesToSkip[]{
    ax::mojom::Role::kAudio,
    ax::mojom::Role::kBanner,
    ax::mojom::Role::kButton,
    ax::mojom::Role::kComplementary,
    ax::mojom::Role::kContentInfo,
    ax::mojom::Role::kFooter,
    ax::mojom::Role::kFooterAsNonLandmark,
    ax::mojom::Role::kHeader,
    ax::mojom::Role::kHeaderAsNonLandmark,
    ax::mojom::Role::kImage,
    ax::mojom::Role::kLabelText,
    ax::mojom::Role::kNavigation,
};
static constexpr int kMaxNodes = 5000;

int32_t GetNumberOfChildParagraphs(const ui::AXNode* node) {
  int n = 0;
  for (auto iter = node->UnignoredChildrenBegin();
       iter != node->UnignoredChildrenEnd(); ++iter) {
    if (iter.get()->GetRole() == ax::mojom::Role::kParagraph)
      n++;
  }
  return n;
}

// TODO(crbug.com/1266555): Replace this with a call to
// OneShotAccessibilityTreeSearch.
// The article node is the one which contains the most number of paragraphs.
// TODO(crbug.com/1266555): Ensure that this also includes the header node.
const ui::AXNode* GetArticleNode(const ui::AXNode* node) {
  const ui::AXNode* article = nullptr;
  int32_t max = 0;
  std::queue<const ui::AXNode*> queue;
  queue.push(node);

  while (!queue.empty()) {
    const ui::AXNode* popped = queue.front();
    queue.pop();
    int32_t n = GetNumberOfChildParagraphs(popped);
    if (n > max) {
      max = n;
      article = popped;
    }

    // TODO(crbug.com/1266555): Replace this with skipping all content nodes,
    // including paragraphs, headings, and other text-like containers from
    // AXNodeProperties.
    if (popped->GetRole() != ax::mojom::Role::kParagraph) {
      // Only explore branches that do not have a large number of paragraphs.
      for (auto iter = popped->UnignoredChildrenBegin();
           iter != popped->UnignoredChildrenEnd(); ++iter) {
        queue.push(iter.get());
      }
    }
  }

  return article;
}

void AddTextNodesToVector(const ui::AXNode* node,
                          std::vector<ui::AXNodeID>* text_node_ids) {
  if (node->GetRole() == ax::mojom::Role::kStaticText) {
    if (node->HasStringAttribute(ax::mojom::StringAttribute::kName))
      text_node_ids->emplace_back(node->id());
    return;
  }

  for (const auto role : kRolesToSkip) {
    if (role == node->GetRole())
      return;
  }
  for (auto iter = node->UnignoredChildrenBegin();
       iter != node->UnignoredChildrenEnd(); ++iter) {
    AddTextNodesToVector(iter.get(), text_node_ids);
  }
}

}  // namespace

namespace content {

AXTreeDistiller::AXTreeDistiller(RenderFrameImpl* render_frame)
    : render_frame_(render_frame) {}

AXTreeDistiller::~AXTreeDistiller() = default;

void AXTreeDistiller::Distill() {
  SnapshotAXTree();
  DistillAXTree();
}

void AXTreeDistiller::SnapshotAXTree() {
  // If snapshot_ is already cached, do nothing.
  if (snapshot_)
    return;
  snapshot_ = std::make_unique<ui::AXTreeUpdate>();

  // Get page contents (via snapshot of a11y tree) for reader generation.
  AXTreeSnapshotterImpl snapshotter(render_frame_, ui::AXMode::kWebContents);
  snapshotter.Snapshot(
      /* exclude_offscreen= */ false, kMaxNodes,
      /* timeout= */ {}, snapshot_.get());
}

void AXTreeDistiller::DistillAXTree() {
  // If text_node_ids_ is already cached, do nothing.
  if (text_node_ids_)
    return;
  text_node_ids_ = std::make_unique<std::vector<ui::AXNodeID>>();

  DCHECK(snapshot_);
  ui::AXTree tree;
  bool success = tree.Unserialize(*snapshot_);
  if (!success)
    return;

  const ui::AXNode* article_node = GetArticleNode(tree.root());
  // If this page does not have an article node, this means it is not
  // distillable.
  if (!article_node) {
    is_distillable_ = false;
    return;
  }

  text_node_ids_->reserve(snapshot_->nodes.size());
  AddTextNodesToVector(article_node, text_node_ids_.get());
}

}  // namespace content
