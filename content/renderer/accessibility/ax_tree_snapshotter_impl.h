// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_ACCESSIBILITY_AX_TREE_SNAPSHOTTER_IMPL_H_
#define CONTENT_RENDERER_ACCESSIBILITY_AX_TREE_SNAPSHOTTER_IMPL_H_

#include <memory>

#include "base/time/time.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "ui/accessibility/ax_tree_update_forward.h"

namespace blink {
class WebAXContext;
}

namespace content {

class RenderFrameImpl;

class AXTreeSnapshotterImpl : public AXTreeSnapshotter,
                              public content::RenderFrameObserver {
 public:
  AXTreeSnapshotterImpl(RenderFrameImpl* render_frame, ui::AXMode ax_mode);
  ~AXTreeSnapshotterImpl() override;

  // AXTreeSnapshotter:
  void Snapshot(size_t max_node_count,
                base::TimeDelta timeout,
                ui::AXTreeUpdate* accessibility_tree) override;

  // RenderFrameObserver:
  void OnDestruct() override;

 private:
  bool SerializeTreeWithLimits(size_t max_node_count,
                               base::TimeDelta timeout,
                               ui::AXTreeUpdate* response);
  bool SerializeTree(ui::AXTreeUpdate* response);

  std::unique_ptr<blink::WebAXContext> context_;

  AXTreeSnapshotterImpl(const AXTreeSnapshotterImpl&) = delete;
  AXTreeSnapshotterImpl& operator=(const AXTreeSnapshotterImpl&) = delete;
};

}  // namespace content

#endif  // CONTENT_RENDERER_ACCESSIBILITY_AX_TREE_SNAPSHOTTER_IMPL_H_
