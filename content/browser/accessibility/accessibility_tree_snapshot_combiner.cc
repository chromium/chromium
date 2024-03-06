// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_snapshot_combiner.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"

namespace content {

AccessibilityTreeSnapshotCombiner::AccessibilityTreeSnapshotCombiner(
    base::OnceCallback<void(const ui::AXTreeUpdate&)> callback,
    mojom::SnapshotAccessibilityTreeParamsPtr params)
    : callback_(std::move(callback)),
      params_(std::move(params)),
      weak_ptr_factory_(this) {}

void AccessibilityTreeSnapshotCombiner::RequestSnapshotOnRenderFrameHost(
    RenderFrameHostImpl* rfhi) {
  rfhi->RequestAXTreeSnapshot(
      base::BindOnce(&AccessibilityTreeSnapshotCombiner::
                         ReceiveSnapshotFromRenderFrameHost,
                     weak_ptr_factory_.GetWeakPtr(),
                     rfhi->AccessibilityIsRootFrame()),
      params_.Clone());
}

void AccessibilityTreeSnapshotCombiner::ReceiveSnapshotFromRenderFrameHost(
    bool is_root_frame,
    const ui::AXTreeUpdate& snapshot) {
  combiner_.AddTree(snapshot, is_root_frame);
}

// This is called automatically after the last call to
// ReceiveSnapshotFromRenderFrameHost when there are no more references to this
// object.
AccessibilityTreeSnapshotCombiner::~AccessibilityTreeSnapshotCombiner() {
  combiner_.Combine();
  std::move(callback_).Run(combiner_.combined());
}

}  // namespace content
