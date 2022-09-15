// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ANDROID_RENDER_WIDGET_HOST_CONNECTOR_BROWSERTEST_H_
#define CONTENT_BROWSER_ANDROID_RENDER_WIDGET_HOST_CONNECTOR_BROWSERTEST_H_

#include "content/browser/android/ime_adapter_android.h"
#include "content/browser/renderer_host/render_widget_host_view_android.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"

namespace content {

class RenderWidgetHostConnectorTest : public ContentBrowserTest {
 public:
  RenderWidgetHostConnectorTest();

  RenderWidgetHostConnectorTest(const RenderWidgetHostConnectorTest&) = delete;
  RenderWidgetHostConnectorTest& operator=(
      const RenderWidgetHostConnectorTest&) = delete;

 protected:
  void SetUpOnMainThread() override;

  WebContentsImpl* web_contents() const {
    return static_cast<WebContentsImpl*>(shell()->web_contents());
  }

  RenderWidgetHostViewAndroid* render_widget_host_view_android() const {
    return static_cast<RenderWidgetHostViewAndroid*>(
        web_contents()->GetRenderWidgetHostView());
  }

  RenderWidgetHostConnector* render_widget_host_connector() const {
    return connector_in_rwhva(render_widget_host_view_android());
  }

  RenderWidgetHostConnector* connector_in_rwhva(
      RenderWidgetHostViewAndroid* rwhva) const {
    // Use ImeAdapterAndroid that inherits RenderWidgetHostConnector for
    // testing.
    return rwhva->ime_adapter_for_testing();
  }
};

}  // namespace content

#endif  // CONTENT_BROWSER_ANDROID_RENDER_WIDGET_HOST_CONNECTOR_BROWSERTEST_H_
