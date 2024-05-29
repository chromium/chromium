// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_CONTENT_INIT_IOS_CONTENT_RENDERER_CLIENT_H_
#define IOS_WEB_CONTENT_INIT_IOS_CONTENT_RENDERER_CLIENT_H_

#import "build/blink_buildflags.h"
#import "content/public/renderer/content_renderer_client.h"

#if !BUILDFLAG(USE_BLINK)
#error File can only be included when USE_BLINK is true
#endif

namespace web {

class IOSContentRendererClient : public content::ContentRendererClient {
 public:
  IOSContentRendererClient();
  IOSContentRendererClient(const IOSContentRendererClient&) = delete;
  IOSContentRendererClient& operator=(const IOSContentRendererClient&) = delete;
  ~IOSContentRendererClient() override;

  // content::ContentRendererClient implementation:
  void RenderThreadStarted() override;
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  void RunScriptsAtDocumentStart(content::RenderFrame* render_frame) override;
  void RunScriptsAtDocumentEnd(content::RenderFrame* render_frame) override;
  void PrepareErrorPage(content::RenderFrame* render_frame,
                        const blink::WebURLError& error,
                        const std::string& http_method,
                        content::mojom::AlternativeErrorPageOverrideInfoPtr
                            alternative_error_page_info,
                        std::string* error_html) override;
  void PrepareErrorPageForHttpStatusError(
      content::RenderFrame* render_frame,
      const blink::WebURLError& error,
      const std::string& http_method,
      int http_status,
      content::mojom::AlternativeErrorPageOverrideInfoPtr
          alternative_error_page_info,
      std::string* error_html) override;
};

}  // namespace web

#endif  // IOS_WEB_CONTENT_INIT_IOS_CONTENT_RENDERER_CLIENT_H_
