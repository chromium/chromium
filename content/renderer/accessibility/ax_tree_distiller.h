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
//  frame. The main API is AXTreeDistiller::Distill(), which kicks off the
//  snapshotting and distillation. Once a distilled AXTree is ready, calls a
//  callback which had been passed in from the render frame.
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

  // Snapshot and distill an AXTree on this render frame.
  // When |IsReadAnythingWithScreen2xEnabled|, this operation is done in the
  // utility process by Screen2x. Otherwise, it is done by a rules-based
  // algorithm in this process.
  // The general pathway is:
  //   1. Snapshot
  //   2. DistillViaAlgorithm OR DistillViaScreen2x
  //   3. RunCallback
  // This pathway may be called multiple times before it has been completed, so
  // we pass data from one method to the next rather than storing it in this
  // class.
  void Distill(mojom::Frame::SnapshotAndDistillAXTreeCallback callback);

 private:
  // Takes a snapshot of the accessibility tree for |render_frame_|.
  void SnapshotAXTree(ui::AXTreeUpdate* snapshot);

  // Distills the AXTree via a rules-based algorithm.
  void DistillViaAlgorithm(
      mojom::Frame::SnapshotAndDistillAXTreeCallback callback,
      const ui::AXTreeUpdate& snapshot);

  // Runs |callback|, notifying the caller that an AXTree has been distilled.
  // This function is called asynchronously when the AXTree is distilled by
  // Screen2x and synchronously otherwise. It passes |snapshot_| and
  // |content_node_ids_| to |callback_.Run()|, which is defined in the browser
  // process.
  void RunCallback(mojom::Frame::SnapshotAndDistillAXTreeCallback callback,
                   const ui::AXTreeUpdate& snapshot,
                   const std::vector<ui::AXNodeID>& content_node_ids);

  RenderFrameImpl* render_frame_;

#if BUILDFLAG(ENABLE_SCREEN_AI_SERVICE)
  // Passes |snapshot| to the Screen2x ML model, which identifes the main
  // content nodes and calls |ProcessScreen2xResult()| on completion.
  void DistillViaScreen2x(
      mojom::Frame::SnapshotAndDistillAXTreeCallback callback,
      const ui::AXTreeUpdate& snapshot);

  // Called by the Screen2x service from the utility process.
  void ProcessScreen2xResult(
      mojom::Frame::SnapshotAndDistillAXTreeCallback callback,
      const ui::AXTreeUpdate& snapshot,
      const std::vector<ui::AXNodeID>& content_node_ids);

  // The remote of the Screen2x main content extractor. The receiver lives in
  // the utility process.
  mojo::Remote<screen_ai::mojom::Screen2xMainContentExtractor>
      main_content_extractor_;

  base::WeakPtrFactory<AXTreeDistiller> weak_ptr_factory_{this};
#endif
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_AX_TREE_SNAPSHOTTER_IMPL_H_
