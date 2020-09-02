// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_RENDERER_WEB_FRAME_TEST_PROXY_H_
#define CONTENT_WEB_TEST_RENDERER_WEB_FRAME_TEST_PROXY_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "content/renderer/render_frame_impl.h"
#include "content/web_test/common/web_test.mojom.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "ui/accessibility/ax_event.h"
#include "ui/accessibility/ax_event_intent.h"

namespace content {
class SpellCheckClient;
class TestRunner;
class WebViewTestProxy;
class WebWidgetTestProxy;

// WebFrameTestProxy is used during running web tests instead of a
// RenderFrameImpl to inject test-only behaviour by overriding methods in the
// base class.
class WebFrameTestProxy : public RenderFrameImpl,
                          public mojom::WebTestRenderFrame {
 public:
  explicit WebFrameTestProxy(RenderFrameImpl::CreateParams params);
  ~WebFrameTestProxy() override;

  // RenderFrameImpl overrides.
  void Initialize(blink::WebFrame* parent) override;

  // Reset state between tests.
  void Reset();

  // Returns a frame name that can be used in the output of web tests
  // (the name is derived from the frame's unique name).
  std::string GetFrameNameForWebTests();
  // Returns a description of the frame, including the name from
  // GetFrameNameForWebTests(), that can be used in the output of web
  // tests.
  std::string GetFrameDescriptionForWebTests();

  // Returns the test-subclass of RenderWidget for the local root of this frame.
  WebWidgetTestProxy* GetLocalRootWebWidgetTestProxy();
  // Returns the test-subclass of RenderViewImpl that is hosting this frame's
  // frame tree fragment.
  WebViewTestProxy* GetWebViewTestProxy();

  // WebLocalFrameClient implementation.
  blink::WebPlugin* CreatePlugin(const blink::WebPluginParams& params) override;
  void DidAddMessageToConsole(const blink::WebConsoleMessage& message,
                              const blink::WebString& source_name,
                              unsigned source_line,
                              const blink::WebString& stack_trace) override;
  void DidStartLoading() override;
  void DidStopLoading() override;
  void DidChangeSelection(bool is_selection_empty) override;
  void DidChangeContents() override;
  blink::WebEffectiveConnectionType GetEffectiveConnectionType() override;
  void ShowContextMenu(const blink::WebContextMenuData& context_menu_data,
                       const base::Optional<gfx::Point>&) override;
  void DidDispatchPingLoader(const blink::WebURL& url) override;
  void WillSendRequest(blink::WebURLRequest& request,
                       ForRedirect for_redirect) override;
  void BeginNavigation(std::unique_ptr<blink::WebNavigationInfo> info) override;
  void PostAccessibilityEvent(const ui::AXEvent& event) override;
  void MarkWebAXObjectDirty(const blink::WebAXObject& object,
                            bool subtree) override;
  void CheckIfAudioSinkExistsAndIsAuthorized(
      const blink::WebString& sink_id,
      blink::WebSetSinkIdCompleteCallback completion_callback) override;
  void DidClearWindowObject() override;

 private:
  // mojom::WebTestRenderFrame implementation.
  void SynchronouslyCompositeAfterTest(
      SynchronouslyCompositeAfterTestCallback callback) override;
  void DumpFrameLayout(DumpFrameLayoutCallback callback) override;
  void SetTestConfiguration(mojom::WebTestRunTestConfigurationPtr config,
                            bool starting_test) override;

  void BindReceiver(
      mojo::PendingAssociatedReceiver<mojom::WebTestRenderFrame> receiver);

  void HandleWebAccessibilityEvent(
      const blink::WebAXObject& object,
      const char* event_name,
      const std::vector<ui::AXEventIntent>& event_intents);

  TestRunner* test_runner();

  WebViewTestProxy* const web_view_test_proxy_;

  std::unique_ptr<SpellCheckClient> spell_check_;

  mojo::AssociatedReceiver<mojom::WebTestRenderFrame>
      web_test_render_frame_receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(WebFrameTestProxy);
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_RENDERER_WEB_FRAME_TEST_PROXY_H_
