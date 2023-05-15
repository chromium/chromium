// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/init/ios_content_renderer_client.h"

#import "components/js_injection/renderer/js_communication.h"

namespace web {

IOSContentRendererClient::IOSContentRendererClient() = default;
IOSContentRendererClient::~IOSContentRendererClient() = default;

void IOSContentRendererClient::RenderThreadStarted() {
  // TODO(crbug.com/1423527): Create and register a v8::Extension for receiving
  // messages from JavaScript.
}

void IOSContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  // As a RenderFrameObserver, JSCommunication handles destroying
  // itself when its RenderFrame is destroyed.
  new js_injection::JsCommunication(render_frame);
}

void IOSContentRendererClient::RunScriptsAtDocumentStart(
    content::RenderFrame* render_frame) {
  js_injection::JsCommunication* communication =
      js_injection::JsCommunication::Get(render_frame);
  communication->RunScriptsAtDocumentStart();
}

void IOSContentRendererClient::RunScriptsAtDocumentEnd(
    content::RenderFrame* render_frame) {
  // TODO(crbug.com/1423527): Inject document end scripts from
  // JavaScriptFeatures.
}

}  // namespace web
