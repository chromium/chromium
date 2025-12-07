// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_SNAPSHOT_COMBINER_H_
#define CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_SNAPSHOT_COMBINER_H_

#include "base/memory/ref_counted.h"
#include "content/common/frame.mojom.h"
#include "ui/accessibility/ax_tree_combiner.h"

namespace content {

class RenderFrameHostImpl;

// Helper class for use during one-off accessibility tree snapshots. This class
// combines AXTreeUpdates into a single AXTreeUpdate using the AXTreeCombiner.
// The class is RefCounted, and when there are no more references and the
// destructor is called, the class will call AXTreeCombiner::Combine and pass
// the resulting AXTreeUpdate to the provided callback.
class AccessibilityTreeSnapshotCombiner
    : public base::RefCounted<AccessibilityTreeSnapshotCombiner> {
 public:
  AccessibilityTreeSnapshotCombiner(
      base::OnceCallback<void(ui::AXTreeUpdate&)> callback,
      mojom::SnapshotAccessibilityTreeParamsPtr params);

  // This method will request a snapshot for the given RenderFrameHostImpl. The
  // snapshot will be completed asynchronously, and when |this| is destroyed,
  // the AXTreeUpdate for this snapshot will be combined with any others into a
  // single update that is passed to the callback provided in the constructor.
  void RequestSnapshotOnRenderFrameHost(RenderFrameHostImpl* rfhi);

 private:
  friend class base::RefCounted<AccessibilityTreeSnapshotCombiner>;

  ~AccessibilityTreeSnapshotCombiner();

  void ReceiveSnapshotFromRenderFrameHost(bool is_root,
                                          ui::AXTreeUpdate& snapshot);

  ui::AXTreeCombiner combiner_;
  base::OnceCallback<void(ui::AXTreeUpdate&)> callback_;
  mojom::SnapshotAccessibilityTreeParamsPtr params_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_ACCESSIBILITY_TREE_SNAPSHOT_COMBINER_H_
