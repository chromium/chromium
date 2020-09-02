// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WEB_TEST_RENDERER_WEB_TEST_CONTENT_RENDERER_CLIENT_H_
#define CONTENT_WEB_TEST_RENDERER_WEB_TEST_CONTENT_RENDERER_CLIENT_H_

#include <memory>

#include "content/shell/renderer/shell_content_renderer_client.h"

namespace content {

class WebTestRenderThreadObserver;

class WebTestContentRendererClient : public ShellContentRendererClient {
 public:
  WebTestContentRendererClient();
  ~WebTestContentRendererClient() override;

  // ShellContentRendererClient implementation.
  void RenderThreadStarted() override;
  void RenderFrameCreated(RenderFrame* render_frame) override;
  std::unique_ptr<content::WebSocketHandshakeThrottleProvider>
  CreateWebSocketHandshakeThrottleProvider() override;
  void DidInitializeWorkerContextOnWorkerThread(
      v8::Local<v8::Context> context) override;
  void SetRuntimeFeaturesDefaultsBeforeBlinkInitialization() override;
  bool IsIdleMediaSuspendEnabled() override;

 private:
  std::unique_ptr<WebTestRenderThreadObserver> render_thread_observer_;
};

}  // namespace content

#endif  // CONTENT_WEB_TEST_RENDERER_WEB_TEST_CONTENT_RENDERER_CLIENT_H_
