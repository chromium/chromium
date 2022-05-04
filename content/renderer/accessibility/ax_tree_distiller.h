// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_AX_TREE_DISTILLER_H_
#define CONTENT_RENDERER_ACCESSIBILITY_AX_TREE_DISTILLER_H_

#include <memory>
#include <vector>

#include "components/services/screen_ai/buildflags/buildflags.h"
#include "content/common/content_export.h"
#include "ui/accessibility/ax_node_id_forward.h"
#include "ui/accessibility/ax_tree_update_forward.h"

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
#include "base/memory/weak_ptr.h"
#include "components/services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#endif

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

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  void ScheduleScreen2xRun();
  void ProcessScreen2xResult(const std::vector<ui::AXNodeID>& content_node_ids);

  mojo::Remote<screen_ai::mojom::Screen2xMainContentExtractor>
      main_content_extractor_;

  base::WeakPtrFactory<AXTreeDistiller> weak_ptr_factory_{this};
#endif
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_AX_TREE_SNAPSHOTTER_IMPL_H_
