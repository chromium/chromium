// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/content/init/ios_content_renderer_client.h"

namespace web {

IOSContentRendererClient::IOSContentRendererClient() = default;
IOSContentRendererClient::~IOSContentRendererClient() = default;

void IOSContentRendererClient::RenderThreadStarted() {
  // TODO(crbug.com/1423527): Create and register a v8::Extension for receiving
  // messages from JavaScript.
}

void IOSContentRendererClient::RunScriptsAtDocumentStart(
    content::RenderFrame* render_frame) {
  // TODO(crbug.com/1423527): Inject document start scripts from
  // JavaScriptFeatures.
}

void IOSContentRendererClient::RunScriptsAtDocumentEnd(
    content::RenderFrame* render_frame) {
  // TODO(crbug.com/1423527): Inject document end scripts from
  // JavaScriptFeatures.
}

}  // namespace web
