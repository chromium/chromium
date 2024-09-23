// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_STUB_RENDER_WIDGET_HOST_OWNER_DELEGATE_H_
#define CONTENT_TEST_STUB_RENDER_WIDGET_HOST_OWNER_DELEGATE_H_

#include "content/browser/renderer_host/render_widget_host_owner_delegate.h"

namespace content {

class StubRenderWidgetHostOwnerDelegate : public RenderWidgetHostOwnerDelegate {
 public:
  void RenderWidgetGotFocus() override {}
  void RenderWidgetLostFocus() override {}
  void RenderWidgetDidForwardMouseEvent(
      const blink::WebMouseEvent& mouse_event) override {}
  bool MayRenderWidgetForwardKeyboardEvent(
      const input::NativeWebKeyboardEvent& key_event) override;
  bool ShouldContributePriorityToProcess() override;
  void SetBackgroundOpaque(bool opaque) override {}
  bool IsMainFrameActive() override;
  bool IsNeverComposited() override;
  blink::web_pref::WebPreferences GetWebkitPreferencesForWidget() override;
};

}  // namespace content

#endif  // CONTENT_TEST_STUB_RENDER_WIDGET_HOST_OWNER_DELEGATE_H_
