// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_AX_TREE_DISTILLER_H_
#define CONTENT_RENDERER_ACCESSIBILITY_AX_TREE_DISTILLER_H_

#include <memory>
#include <vector>

#include "base/callback.h"
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

// From content/common/frame.mojom:
using SnapshotAndDistillAXTreeCallback =
    base::OnceCallback<void(const ui::AXTreeUpdate&,
                            const std::vector<int32_t>&)>;

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

  void Distill(SnapshotAndDistillAXTreeCallback callback);

 private:
  // Takes a snapshot of an accessibility tree and caches it as |snapshot_|.
  void SnapshotAXTree();

  // Distills |snapshot_| by identifying main content nodes and caching their
  // IDs as |content_node_ids_|.
  void DistillAXTree();

  // Called when the AXTree is distilled. Called asynchronously if Screen2x is
  // running in another process. Runs |callback_| which sends |snapshot_| and
  // |content_node_ids_| across the render frame.
  void OnAXTreeDistilled();

  RenderFrameImpl* render_frame_;
  std::unique_ptr<ui::AXTreeUpdate> snapshot_;
  std::unique_ptr<std::vector<ui::AXNodeID>> content_node_ids_;
  SnapshotAndDistillAXTreeCallback callback_;
  bool is_distillable_ = true;

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  // Passes |snapshot_| to the Screen2x ML model, which identifes the main
  // content nodes and calls |ProcessScreen2xResult()| on completion.
  void ScheduleScreen2xRun();

  // Called by the Screen2x service from the utility process. Caches
  // |content_node_ids| as |content_node_ids_|.
  void ProcessScreen2xResult(const std::vector<ui::AXNodeID>& content_node_ids);

  mojo::Remote<screen_ai::mojom::Screen2xMainContentExtractor>
      main_content_extractor_;

  base::WeakPtrFactory<AXTreeDistiller> weak_ptr_factory_{this};
#endif
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_AX_TREE_SNAPSHOTTER_IMPL_H_
