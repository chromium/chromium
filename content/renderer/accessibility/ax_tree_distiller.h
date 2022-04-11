// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_AX_TREE_DISTILLER_H_
#define CONTENT_RENDERER_ACCESSIBILITY_AX_TREE_DISTILLER_H_

#include <memory>
#include <vector>

#include "content/common/content_export.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_tree_update_forward.h"

namespace content {

class RenderFrameImpl;

///////////////////////////////////////////////////////////////////////////////
// AXTreeDistiller
//
//  A class that creates and stores a distilled AXTree for a particular render
//  frame.
//
class CONTENT_EXPORT AXTreeDistiller {
 public:
  explicit AXTreeDistiller(RenderFrameImpl* render_frame);
  ~AXTreeDistiller();
  AXTreeDistiller(const AXTreeDistiller&) = delete;
  AXTreeDistiller& operator=(const AXTreeDistiller&) = delete;

  void Distill();

  ui::AXTreeUpdate* GetSnapshot() { return snapshot_.get(); }
  std::vector<ui::AXNodeID>* GetContentNodeIDs() {
    return content_node_ids_.get();
  }
  bool IsDistillable() { return is_distillable_; }

 private:
  // Takes a snapshot of an accessibility tree and caches it as |snapshot_|.
  void SnapshotAXTree();

  // Distills |snapshot_| by identifying main content nodes and caching their
  // IDs as |content_node_ids_|.
  void DistillAXTree();

  RenderFrameImpl* render_frame_;
  std::unique_ptr<ui::AXTreeUpdate> snapshot_;
  std::unique_ptr<std::vector<ui::AXNodeID>> content_node_ids_;
  bool is_distillable_ = true;
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_AX_TREE_SNAPSHOTTER_IMPL_H_
