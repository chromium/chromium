// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/accessibility/ax_tree_snapshotter_impl.h"

#include "content/renderer/accessibility/blink_ax_tree_source.h"
#include "content/renderer/render_frame_impl.h"
#include "third_party/blink/public/web/web_ax_context.h"
#include "third_party/blink/public/web/web_ax_object.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/ax_tree_update.h"

using blink::WebAXContext;
using blink::WebAXObject;
using blink::WebDocument;
using BlinkAXTreeSerializer = ui::AXTreeSerializer<blink::WebAXObject>;

namespace content {

AXTreeSnapshotterImpl::AXTreeSnapshotterImpl(RenderFrameImpl* render_frame,
                                             ui::AXMode ax_mode)
    : render_frame_(render_frame) {
  DCHECK(render_frame->GetWebFrame());
  blink::WebDocument document_ = render_frame->GetWebFrame()->GetDocument();
  context_ = std::make_unique<WebAXContext>(document_, ax_mode);
}

AXTreeSnapshotterImpl::~AXTreeSnapshotterImpl() = default;

void AXTreeSnapshotterImpl::Snapshot(bool exclude_offscreen,
                                     size_t max_node_count,
                                     base::TimeDelta timeout,
                                     ui::AXTreeUpdate* response) {
  if (!render_frame_->GetWebFrame())
    return;
  if (!WebAXObject::MaybeUpdateLayoutAndCheckValidity(
          render_frame_->GetWebFrame()->GetDocument()))
    return;
  WebAXObject root = context_->Root();

  BlinkAXTreeSource tree_source(render_frame_, context_->GetAXMode());
  tree_source.SetRoot(root);
  tree_source.set_exclude_offscreen(exclude_offscreen);
  ScopedFreezeBlinkAXTreeSource freeze(&tree_source);

  // The serializer returns an ui::AXTreeUpdate, which can store a complete
  // or a partial accessibility tree. AXTreeSerializer is stateful, but the
  // first time you serialize from a brand-new tree you're guaranteed to get a
  // complete tree.
  BlinkAXTreeSerializer serializer(&tree_source);
  if (max_node_count)
    serializer.set_max_node_count(max_node_count);
  if (!timeout.is_zero())
    serializer.set_timeout(timeout);
  if (serializer.SerializeChanges(root, response))
    return;

  // It's possible for the page to fail to serialize the first time due to
  // aria-owns rearranging the page while it's being scanned. Try a second
  // time.
  *response = ui::AXTreeUpdate();
  if (serializer.SerializeChanges(root, response))
    return;

  // It failed again. Clear the response object because it might have errors.
  *response = ui::AXTreeUpdate();
  LOG(WARNING) << "Unable to serialize accessibility tree.";

  // As a sanity check, node_id_to_clear and event_from should be uninitialized
  // if this is a full tree snapshot. They'd only be set to something if
  // this was indeed a partial update to the tree (which we don't want).
  DCHECK_EQ(0, response->node_id_to_clear);
  DCHECK_EQ(ax::mojom::EventFrom::kNone, response->event_from);
  DCHECK_EQ(ax::mojom::Action::kNone, response->event_from_action);
}

}  // namespace content
