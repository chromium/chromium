// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_HIT_TEST_DEBUG_KEY_EVENT_OBSERVER_H_
#define CONTENT_BROWSER_RENDERER_HOST_HIT_TEST_DEBUG_KEY_EVENT_OBSERVER_H_

#include "content/public/browser/render_widget_host.h"

namespace viz {

class HitTestQuery;

}  // namespace viz

namespace content {

class RenderWidgetHostImpl;

// Implements the RenderWidgetHost::InputEventObserver interface, and acts on
// keyboard input events to print hit-test data.
class HitTestDebugKeyEventObserver
    : public RenderWidgetHost::InputEventObserver {
 public:
  explicit HitTestDebugKeyEventObserver(RenderWidgetHostImpl* host);
  ~HitTestDebugKeyEventObserver() override;

  // RenderWidgetHost::InputEventObserver:
  void OnInputEventAck(InputEventAckSource source,
                       InputEventAckState state,
                       const blink::WebInputEvent&) override;

 private:
  RenderWidgetHostImpl* host_;
  viz::HitTestQuery* hit_test_query_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_HIT_TEST_DEBUG_KEY_EVENT_OBSERVER_H_
