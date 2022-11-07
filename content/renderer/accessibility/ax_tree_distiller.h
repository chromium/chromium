// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_AX_TREE_DISTILLER_H_
#define CONTENT_RENDERER_ACCESSIBILITY_AX_TREE_DISTILLER_H_

#include <memory>
#include <vector>

#include "components/services/screen_ai/buildflags/buildflags.h"
#include "content/common/content_export.h"
#include "content/common/frame.mojom.h"
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
//  frame. The main API is AXTreeDistiller::Distill(), which stores the passed-
//  in callback and kicks off the distillation. Once a distilled AXTree is
//  ready, the callback is called.
//  When |IsReadAnythingWithScreen2xEnabled()|, the distillation is performed
//  by the Screen2x ML model in the utility process. Otherwise, distillation is
//  done using rules defined in this file.
//
class CONTENT_EXPORT AXTreeDistiller {
 public:
  explicit AXTreeDistiller(RenderFrameImpl* render_frame);
  ~AXTreeDistiller();
  AXTreeDistiller(const AXTreeDistiller&) = delete;
  AXTreeDistiller& operator=(const AXTreeDistiller&) = delete;

  // Snapshot and distill an AXTree on this render frame. Saves callback as
  // |callback_|.
  void Distill(mojom::Frame::SnapshotAndDistillAXTreeCallback callback);

 private:
  // Does nothing if |snapshot_| is already defined. Otherwise, takes a snapshot
  // of the accessibility tree for |render_frame_| and caches it as |snapshot_|.
  void SnapshotAXTree();

  // If |content_node_ids_| is already defined, notifies the handler that the
  // AXTree has already been distilled. Otherwise, distills |snapshot_| by
  // identifying main content nodes and caching their IDs as
  // |content_node_ids_|. When |IsReadAnythingWithScreen2xEnabled|, this
  // process is done in the utility process by Screen2x. Otherwise, it is done
  // by a rules-based algorithm in this process.
  void DistillAXTree();

  // Distills the AXTree via a rules-based algorithm.
  void DistillViaAlgorithm();

  // Run the callback, notifying the caller that an AXTree has been distilled.
  // This function is called asynchronously when the AXTree is distilled by
  // Screen2x and synchronously otherwise. It passes |snapshot_| and
  // |content_node_ids_| to |callback_.Run()|, which is defined in the browser
  // process.
  void RunCallback();

  RenderFrameImpl* render_frame_;
  std::unique_ptr<ui::AXTreeUpdate> snapshot_;
  std::unique_ptr<std::vector<ui::AXNodeID>> content_node_ids_;

  // A function  defined in the browser process and passed across the render
  // frame to AXTreeDistiller.
  mojom::Frame::SnapshotAndDistillAXTreeCallback callback_;

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  // Passes |snapshot_| to the Screen2x ML model, which identifes the main
  // content nodes and calls |ProcessScreen2xResult()| on completion.
  void ScheduleScreen2xRun();

  // Called by the Screen2x service from the utility process. Caches
  // |content_node_ids| as |content_node_ids_|.
  void ProcessScreen2xResult(const std::vector<ui::AXNodeID>& content_node_ids);

  // The remote of the Screen2x main content extractor. The receiver lives in
  // the utility process.
  mojo::Remote<screen_ai::mojom::Screen2xMainContentExtractor>
      main_content_extractor_;

  base::WeakPtrFactory<AXTreeDistiller> weak_ptr_factory_{this};
#endif
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_AX_TREE_SNAPSHOTTER_IMPL_H_
