// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/init/ios_content_renderer_client.h"

#import "components/js_injection/renderer/js_communication.h"

namespace web {

IOSContentRendererClient::IOSContentRendererClient() = default;
IOSContentRendererClient::~IOSContentRendererClient() = default;

void IOSContentRendererClient::RenderThreadStarted() {
  // TODO(crbug.com/40260088): Create and register a v8::Extension for receiving
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
  // TODO(crbug.com/40260088): Inject document end scripts from
  // JavaScriptFeatures.
}

void IOSContentRendererClient::PrepareErrorPage(
    content::RenderFrame* render_frame,
    const blink::WebURLError& error,
    const std::string& http_method,
    content::mojom::AlternativeErrorPageOverrideInfoPtr
        alternative_error_page_info,
    std::string* error_html) {
  if (error_html) {
    *error_html = "This is an error page";
  }
}

void IOSContentRendererClient::PrepareErrorPageForHttpStatusError(
    content::RenderFrame* render_frame,
    const blink::WebURLError& error,
    const std::string& http_method,
    int http_status,
    content::mojom::AlternativeErrorPageOverrideInfoPtr
        alternative_error_page_info,
    std::string* error_html) {
  if (error_html) {
    *error_html = "This is an http status error page";
  }
}

}  // namespace web
