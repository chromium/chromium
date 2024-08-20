// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_tree_snapshot_combiner.h"

#include "base/metrics/histogram_macros.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"

namespace content {

AccessibilityTreeSnapshotCombiner::AccessibilityTreeSnapshotCombiner(
    base::OnceCallback<void(ui::AXTreeUpdate&)> callback,
    mojom::SnapshotAccessibilityTreeParamsPtr params)
    : callback_(std::move(callback)), params_(std::move(params)) {}

void AccessibilityTreeSnapshotCombiner::RequestSnapshotOnRenderFrameHost(
    RenderFrameHostImpl* rfhi) {
  // The callback creates a reference to this, which is needed to keep the
  // object alive using base::RefCounted. Once all frames have received their
  // responses from the renderer and run their respective callbacks, all
  // references to this will be removed and the destructor will be called.
  rfhi->RequestAXTreeSnapshot(
      base::BindOnce(&AccessibilityTreeSnapshotCombiner::
                         ReceiveSnapshotFromRenderFrameHost,
                     this, rfhi->AccessibilityIsRootFrame()),
      params_.Clone());
}

void AccessibilityTreeSnapshotCombiner::ReceiveSnapshotFromRenderFrameHost(
    bool is_root_frame,
    ui::AXTreeUpdate& snapshot) {
  combiner_.AddTree(snapshot, is_root_frame);
}

// This is called automatically after the last call to
// ReceiveSnapshotFromRenderFrameHost when there are no more references to this
// object.
AccessibilityTreeSnapshotCombiner::~AccessibilityTreeSnapshotCombiner() {
  combiner_.Combine();
  CHECK(combiner_.combined());
  ui::AXTreeUpdate update = std::move(combiner_.combined().value());

  // This ensures a move of `combiner_.combined()`. It should be safe to steal
  // `combiner_`'s resources since we're being destroyed.
  std::move(callback_).Run(update);
}

}  // namespace content
