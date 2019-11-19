// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_FRAME_TREE_NODE_BLAME_CONTEXT_H
#define CONTENT_BROWSER_FRAME_HOST_FRAME_TREE_NODE_BLAME_CONTEXT_H

#include "base/macros.h"
#include "base/trace_event/blame_context.h"
#include "url/gurl.h"

namespace base {
namespace trace_event {
class TracedValue;
}  // namespace trace_event
}  // namespace base

namespace content {

class FrameTreeNode;

// FrameTreeNodeBlameContext is a helper class for tracing snapshots of each
// FrameTreeNode and attributing browser activities to frames (when possible),
// in the framework of FrameBlamer (crbug.com/546021).  This class is unrelated
// to the core logic of FrameTreeNode.
class FrameTreeNodeBlameContext : public base::trace_event::BlameContext {
 public:
  FrameTreeNodeBlameContext(int node_id, FrameTreeNode* parent);
  ~FrameTreeNodeBlameContext() override;

 private:
  void AsValueInto(base::trace_event::TracedValue* value) override;

  DISALLOW_COPY_AND_ASSIGN(FrameTreeNodeBlameContext);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_FRAME_TREE_NODE_BLAME_CONTEXT_H
