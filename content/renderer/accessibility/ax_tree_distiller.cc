// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/ax_tree_distiller.h"

#include <memory>
#include <queue>
#include <utility>
#include <vector>

#include "base/containers/contains.h"
#include "base/strings/utf_string_conversions.h"
#include "content/renderer/accessibility/ax_tree_snapshotter_impl.h"
#include "content/renderer/render_frame_impl.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_tree.h"

namespace {

// TODO: Consider moving this to AXNodeProperties.
static const ax::mojom::Role kContentRoles[]{
    ax::mojom::Role::kHeading,
    ax::mojom::Role::kParagraph,
};

// TODO: Consider moving this to AXNodeProperties.
static const ax::mojom::Role kRolesToSkip[]{
    ax::mojom::Role::kAudio,
    ax::mojom::Role::kBanner,
    ax::mojom::Role::kButton,
    ax::mojom::Role::kComplementary,
    ax::mojom::Role::kContentInfo,
    ax::mojom::Role::kFooter,
    ax::mojom::Role::kFooterAsNonLandmark,
    ax::mojom::Role::kImage,
    ax::mojom::Role::kLabelText,
    ax::mojom::Role::kNavigation,
};

// Find all of the main and article nodes.
// TODO(crbug.com/1266555): Replace this with a call to
// OneShotAccessibilityTreeSearch.
void GetContentRootNodes(const ui::AXNode* root,
                         std::vector<const ui::AXNode*>* content_root_nodes) {
  std::queue<const ui::AXNode*> queue;
  queue.push(root);
  while (!queue.empty()) {
    const ui::AXNode* node = queue.front();
    queue.pop();
    // If a main or article node is found, add it to the list of content root
    // nodes and continue. Do not explore children for nested article nodes.
    if (node->GetRole() == ax::mojom::Role::kMain ||
        node->GetRole() == ax::mojom::Role::kArticle) {
      content_root_nodes->push_back(node);
      continue;
    }
    for (auto iter = node->UnignoredChildrenBegin();
         iter != node->UnignoredChildrenEnd(); ++iter) {
      queue.push(iter.get());
    }
  }
}

// Recurse through the root node, searching for content nodes (any node whose
// role is in kContentRoles). Skip branches which begin with a node with role
// in kRolesToSkip. Once a content node is identified, add it to the vector
// |content_node_ids|, whose pointer is passed through the recursion.
void AddContentNodesToVector(const ui::AXNode* node,
                             std::vector<ui::AXNodeID>* content_node_ids) {
  if (base::Contains(kContentRoles, node->GetRole())) {
    content_node_ids->emplace_back(node->id());
    return;
  }
  if (base::Contains(kRolesToSkip, node->GetRole()))
    return;
  for (auto iter = node->UnignoredChildrenBegin();
       iter != node->UnignoredChildrenEnd(); ++iter) {
    AddContentNodesToVector(iter.get(), content_node_ids);
  }
}

}  // namespace

namespace content {

AXTreeDistiller::AXTreeDistiller(RenderFrameImpl* render_frame)
    : render_frame_(render_frame) {
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  if (features::IsReadAnythingWithScreen2xEnabled()) {
    render_frame_->GetBrowserInterfaceBroker()->GetInterface(
        main_content_extractor_.BindNewPipeAndPassReceiver());
  }
#endif
}

AXTreeDistiller::~AXTreeDistiller() = default;

void AXTreeDistiller::Distill(
    mojom::Frame::SnapshotAndDistillAXTreeCallback callback) {
  ui::AXTreeUpdate snapshot;
  SnapshotAXTree(&snapshot);

  // If Read Anything with Screen 2x is enabled, kick off Screen 2x run, which
  // distills the AXTree in the utility process using ML.
#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  if (features::IsReadAnythingWithScreen2xEnabled()) {
    DistillViaScreen2x(std::move(callback), snapshot);
    return;
  }
#endif

  // Otherwise, distill the AXTree in process using the rules-based algorithm.
  DistillViaAlgorithm(std::move(callback), snapshot);
}

void AXTreeDistiller::SnapshotAXTree(ui::AXTreeUpdate* snapshot) {
  // Get page contents (via snapshot of a11y tree) for reader generation.
  // |ui::AXMode::kHTML| is needed for URL information.
  // |ui::AXMode::kScreenReader| is needed for heading level information.
  ui::AXMode ax_mode =
      ui::AXMode::kWebContents | ui::AXMode::kHTML | ui::AXMode::kScreenReader;
  AXTreeSnapshotterImpl snapshotter(render_frame_, ax_mode);
  // Setting max_node_count = 0 means there is no max.
  // TODO(crbug.com/1266555): Set a timeout to ensure that huge pages do not
  // cause the snapshotter to hang.
  snapshotter.Snapshot(
      /* exclude_offscreen= */ false, /* max_node_count= */ 0,
      /* timeout= */ {}, snapshot);
}

void AXTreeDistiller::DistillViaAlgorithm(
    mojom::Frame::SnapshotAndDistillAXTreeCallback callback,
    const ui::AXTreeUpdate& snapshot) {
  // Unserialize the snapshot. Failure to unserialize doesn't result in a crash:
  // we control both ends of the serialization-unserialization so any failures
  // are programming error.
  ui::AXTree tree;
  if (!tree.Unserialize(snapshot))
    NOTREACHED() << tree.error();

  std::vector<const ui::AXNode*> content_root_nodes;
  std::vector<ui::AXNodeID> content_node_ids;
  GetContentRootNodes(tree.root(), &content_root_nodes);
  for (const ui::AXNode* content_root_node : content_root_nodes) {
    AddContentNodesToVector(content_root_node, &content_node_ids);
  }
  RunCallback(std::move(callback), snapshot, content_node_ids);
}

void AXTreeDistiller::RunCallback(
    mojom::Frame::SnapshotAndDistillAXTreeCallback callback,
    const ui::AXTreeUpdate& snapshot,
    const std::vector<ui::AXNodeID>& content_node_ids) {
  std::move(callback).Run(snapshot, content_node_ids);
}

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
void AXTreeDistiller::DistillViaScreen2x(
    mojom::Frame::SnapshotAndDistillAXTreeCallback callback,
    const ui::AXTreeUpdate& snapshot) {
  DCHECK(main_content_extractor_.is_bound());
  ui::AXTreeUpdate snapshot_copy(snapshot);
  main_content_extractor_->ExtractMainContent(
      snapshot_copy, base::BindOnce(&AXTreeDistiller::ProcessScreen2xResult,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    std::move(callback), std::move(snapshot)));
}

void AXTreeDistiller::ProcessScreen2xResult(
    mojom::Frame::SnapshotAndDistillAXTreeCallback callback,
    const ui::AXTreeUpdate& snapshot,
    const std::vector<ui::AXNodeID>& content_node_ids) {
  // If content nodes were identified, run callback.
  if (!content_node_ids.empty()) {
    RunCallback(std::move(callback), snapshot, content_node_ids);
    return;
  }

  // Otherwise, try the rules-based approach.
  DistillViaAlgorithm(std::move(callback), snapshot);

  // TODO(crbug.com/1266555): If still no content nodes were identified, and
  // there is a selection, try sending Screen2x a partial tree just containing
  // the selected nodes.
}
#endif

}  // namespace content
