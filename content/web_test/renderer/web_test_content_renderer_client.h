// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_RENDERER_WEB_TEST_CONTENT_RENDERER_CLIENT_H_
#define CONTENT_WEB_TEST_RENDERER_WEB_TEST_CONTENT_RENDERER_CLIENT_H_

#include <memory>

#include "content/shell/renderer/shell_content_renderer_client.h"
#include "third_party/blink/public/web/web_frame_widget.h"

namespace content {

class TestRunner;

class WebTestContentRendererClient : public ShellContentRendererClient {
 public:
  WebTestContentRendererClient();
  ~WebTestContentRendererClient() override;

  // ShellContentRendererClient implementation.
  void RenderThreadStarted() override;
  void RenderFrameCreated(RenderFrame* render_frame) override;
  std::unique_ptr<blink::WebSocketHandshakeThrottleProvider>
  CreateWebSocketHandshakeThrottleProvider() override;
  void DidInitializeWorkerContextOnWorkerThread(
      v8::Local<v8::Context> context) override;
  void SetRuntimeFeaturesDefaultsBeforeBlinkInitialization() override;
  bool IsIdleMediaSuspendEnabled() override;

  TestRunner* test_runner() { return test_runner_.get(); }

 private:
  blink::CreateWebFrameWidgetCallback create_widget_callback_;
  std::unique_ptr<TestRunner> test_runner_;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_RENDERER_WEB_TEST_CONTENT_RENDERER_CLIENT_H_
