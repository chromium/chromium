// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/render_widget_mouse_lock_dispatcher.h"

#include "content/renderer/render_view_impl.h"
#include "third_party/blink/public/mojom/input/pointer_lock_context.mojom.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/public/web/web_widget.h"

namespace content {

RenderWidgetMouseLockDispatcher::RenderWidgetMouseLockDispatcher(
    RenderWidget* render_widget)
    : render_widget_(render_widget) {}

RenderWidgetMouseLockDispatcher::~RenderWidgetMouseLockDispatcher() {}

void RenderWidgetMouseLockDispatcher::SendLockMouseRequest(
    blink::WebLocalFrame* requester_frame,
    bool request_unadjusted_movement) {
  bool has_transient_user_activation =
      requester_frame ? requester_frame->HasTransientUserActivation() : false;
  render_widget_->GetWebWidget()->RequestMouseLock(
      has_transient_user_activation, request_unadjusted_movement,
      base::BindOnce(&RenderWidgetMouseLockDispatcher::OnMouseLocked,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RenderWidgetMouseLockDispatcher::OnMouseLocked(
    blink::mojom::PointerLockResult result,
    blink::CrossVariantMojoRemote<blink::mojom::PointerLockContextInterfaceBase>
        context) {
  // Notify the base class.
  MouseLockDispatcher::OnLockMouseACK(result, std::move(context));

  // Mouse Lock removes the system cursor and provides all mouse motion as
  // .movementX/Y values on events all sent to a fixed target. This requires
  // content to specifically request the mode to be entered.
  // Mouse Capture is implicitly given for the duration of a drag event, and
  // sends all mouse events to the initial target of the drag.
  // If Lock is entered it supersedes any in progress Capture.
  if (result == blink::mojom::PointerLockResult::kSuccess &&
      render_widget_->GetWebWidget())
    render_widget_->GetWebWidget()->MouseCaptureLost();
}

}  // namespace content
