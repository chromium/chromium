// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_TEST_RUNNER_WEB_FRAME_TEST_PROXY_H_
#define CONTENT_SHELL_TEST_RUNNER_WEB_FRAME_TEST_PROXY_H_

#include <memory>
#include <string>
#include <utility>

#include "base/macros.h"
#include "content/renderer/render_frame_impl.h"
#include "content/shell/test_runner/test_runner_export.h"
#include "content/shell/test_runner/web_frame_test_client.h"
#include "third_party/blink/public/platform/web_effective_connection_type.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"

namespace content {
class RenderViewImpl;
}

namespace test_runner {
class WebTestInterfaces;

class TEST_RUNNER_EXPORT WebFrameTestProxyBase {
 public:
  blink::WebLocalFrame* web_frame() const { return web_frame_; }
  void set_web_frame(blink::WebLocalFrame* frame) {
    DCHECK(frame);
    DCHECK(!web_frame_);
    web_frame_ = frame;
  }

 protected:
  WebFrameTestProxyBase() = default;
  ~WebFrameTestProxyBase() = default;

 private:
  blink::WebLocalFrame* web_frame_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WebFrameTestProxyBase);
};

// WebFrameTestProxy is used during LayoutTests instead of a RenderFrameImpl to
// inject test-only behaviour by overriding methods in the base class.
class TEST_RUNNER_EXPORT WebFrameTestProxy : public content::RenderFrameImpl,
                                             public WebFrameTestProxyBase {
 public:
  template <typename... Args>
  explicit WebFrameTestProxy(Args&&... args)
      : RenderFrameImpl(std::forward<Args>(args)...) {}
  ~WebFrameTestProxy() override;

  void Initialize(WebTestInterfaces* interfaces,
                  content::RenderViewImpl* render_view_for_frame);

  // WebLocalFrameClient implementation.
  blink::WebPlugin* CreatePlugin(const blink::WebPluginParams& params) override;
  void DidAddMessageToConsole(const blink::WebConsoleMessage& message,
                              const blink::WebString& source_name,
                              unsigned source_line,
                              const blink::WebString& stack_trace) override;
  void DownloadURL(const blink::WebURLRequest& request,
                   blink::WebLocalFrameClient::CrossOriginRedirects
                       cross_origin_redirect_behavior,
                   mojo::ScopedMessagePipeHandle blob_url_token) override;
  void DidReceiveTitle(const blink::WebString& title,
                       blink::WebTextDirection direction) override;
  void DidChangeIcon(blink::WebIconURL::Type icon_type) override;
  void DidFailLoad(const blink::WebURLError& error,
                   blink::WebHistoryCommitType commit_type) override;
  void DidStartLoading() override;
  void DidStopLoading() override;
  void DidChangeSelection(bool is_selection_empty) override;
  void DidChangeContents() override;
  blink::WebEffectiveConnectionType GetEffectiveConnectionType() override;
  void RunModalAlertDialog(const blink::WebString& message) override;
  bool RunModalConfirmDialog(const blink::WebString& message) override;
  bool RunModalPromptDialog(const blink::WebString& message,
                            const blink::WebString& default_value,
                            blink::WebString* actual_value) override;
  bool RunModalBeforeUnloadDialog(bool is_reload) override;
  void ShowContextMenu(
      const blink::WebContextMenuData& context_menu_data) override;
  void DidDispatchPingLoader(const blink::WebURL& url) override;
  void WillSendRequest(blink::WebURLRequest& request) override;
  void DidReceiveResponse(const blink::WebURLResponse& response) override;
  blink::WebNavigationPolicy DecidePolicyForNavigation(
      const blink::WebLocalFrameClient::NavigationPolicyInfo& info) override;
  void PostAccessibilityEvent(const blink::WebAXObject& object,
                              ax::mojom::Event event) override;
  void MarkWebAXObjectDirty(const blink::WebAXObject& object,
                            bool subtree) override;
  void CheckIfAudioSinkExistsAndIsAuthorized(
      const blink::WebString& sink_id,
      std::unique_ptr<blink::WebSetSinkIdCallbacks> web_callbacks) override;
  void DidClearWindowObject() override;
  bool RunFileChooser(const blink::WebFileChooserParams& params,
                      blink::WebFileChooserCompletion* completion) override;

 private:
  std::unique_ptr<WebFrameTestClient> test_client_;

  DISALLOW_COPY_AND_ASSIGN(WebFrameTestProxy);
};

}  // namespace test_runner

#endif  // CONTENT_SHELL_TEST_RUNNER_WEB_FRAME_TEST_PROXY_H_
