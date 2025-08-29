// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_SELECTION_CONTROLLER_INPUT_OBSERVER_H_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_SELECTION_CONTROLLER_INPUT_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/touch_selection_controller_client_manager.h"
#include "ui/touch_selection/touch_selection_controller.h"

namespace content {

// This class is responsible for forwarding input events to
// TouchSelectionController. TouchSelectionController lives in //ui layer which
// cannot depend on //content so we need to have this bridge which forwards the
// input events.
class TouchSelectionControllerInputObserver
    : public RenderWidgetHost::InputEventObserver {
 public:
  // `controller`, `manager` and TouchSelectionControllerInputObserver are
  // owned by RenderWidgetHostView implementation, and are destroyed in
  // destructor of RenderWidgetHostView. The destruction is managed to destroy
  // TouchSelectionControllerInputObserver after `controller` and `manager`.
  TouchSelectionControllerInputObserver(
      ui::TouchSelectionController* controller,
      TouchSelectionControllerClientManager* manager);

  // Start RenderWidgetHost::InputEventObserver overrides.
  // TODO(crbug.com/375388841): Make touch selection controller an input event
  // observer for Aura as well, currently it's just being used on Android.
  void OnInputEvent(const RenderWidgetHost& widget,
                    const blink::WebInputEvent&) override;
  void OnInputEventAck(const RenderWidgetHost& widget,
                       blink::mojom::InputEventResultSource source,
                       blink::mojom::InputEventResultState state,
                       const blink::WebInputEvent&) override;
  // End RenderWidgetHost::InputEventObserver overrides.

  void SetTouchSelectionControllerForTesting(
      ui::TouchSelectionController* controller) {
    controller_ = controller;
  }

  bool HasSeenScrollBeginAckForTesting() const {
    return has_seen_scroll_begin_ack_;
  }

 private:
  bool has_seen_scroll_begin_ack_ = false;
  raw_ptr<ui::TouchSelectionController> controller_;
  raw_ptr<TouchSelectionControllerClientManager> manager_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_TOUCH_SELECTION_CONTROLLER_INPUT_OBSERVER_H_
