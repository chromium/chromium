// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_AX_TREE_SNAPSHOTTER_IMPL_H_
#define CONTENT_RENDERER_ACCESSIBILITY_AX_TREE_SNAPSHOTTER_IMPL_H_

#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "content/public/renderer/render_frame.h"
#include "ui/accessibility/ax_tree_update_forward.h"

namespace blink {
class WebAXContext;
}

namespace content {

class RenderFrameImpl;

class AXTreeSnapshotterImpl : public AXTreeSnapshotter {
 public:
  AXTreeSnapshotterImpl(RenderFrameImpl* render_frame, ui::AXMode ax_mode);
  ~AXTreeSnapshotterImpl() override;

  // AXTreeSnapshotter implementation.
  void Snapshot(size_t max_node_count,
                base::TimeDelta timeout,
                ui::AXTreeUpdate* accessibility_tree) override;

 private:
  bool SerializeTreeWithLimits(size_t max_node_count,
                               base::TimeDelta timeout,
                               ui::AXTreeUpdate* response);
  bool SerializeTree(ui::AXTreeUpdate* response);

  raw_ptr<RenderFrameImpl> render_frame_;
  std::unique_ptr<blink::WebAXContext> context_;

  AXTreeSnapshotterImpl(const AXTreeSnapshotterImpl&) = delete;
  AXTreeSnapshotterImpl& operator=(const AXTreeSnapshotterImpl&) = delete;
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_AX_TREE_SNAPSHOTTER_IMPL_H_
