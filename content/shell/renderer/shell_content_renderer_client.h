// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_RENDERER_SHELL_CONTENT_RENDERER_CLIENT_H_
#define CONTENT_SHELL_RENDERER_SHELL_CONTENT_RENDERER_CLIENT_H_

#include <memory>
#include <string>

#include "build/build_config.h"
#include "content/public/common/alternative_error_page_override_info.mojom-forward.h"
#include "content/public/renderer/content_renderer_client.h"
#include "media/mojo/buildflags.h"

namespace blink {
class URLLoaderThrottleProvider;
enum class URLLoaderThrottleProviderType;
}  // namespace blink

namespace web_cache {
class WebCacheImpl;
}

namespace content {

class ShellContentRendererClient : public ContentRendererClient {
 public:
  ShellContentRendererClient();
  ~ShellContentRendererClient() override;

  // ContentRendererClient implementation.
  void SetUpWebAssemblyTrapHandler() override;
  void RenderThreadStarted() override;
  void ExposeInterfacesToBrowser(mojo::BinderMap* binders) override;
  void RenderFrameCreated(RenderFrame* render_frame) override;
  void PrepareErrorPage(RenderFrame* render_frame,
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

  void DidInitializeWorkerContextOnWorkerThread(
      v8::Local<v8::Context> context) override;

  std::unique_ptr<blink::URLLoaderThrottleProvider>
  CreateURLLoaderThrottleProvider(
      blink::URLLoaderThrottleProviderType provider_type) override;

#if BUILDFLAG(ENABLE_MOJO_CDM)
  std::unique_ptr<media::KeySystemSupportRegistration> GetSupportedKeySystems(
      content::RenderFrame* render_frame,
      media::GetSupportedKeySystemsCB cb) override;
#endif

  std::unique_ptr<blink::WebPrescientNetworking> CreatePrescientNetworking(
      RenderFrame* render_frame) override;

 private:
  std::unique_ptr<web_cache::WebCacheImpl> web_cache_impl_;
};

}  // namespace content

#endif  // CONTENT_SHELL_RENDERER_SHELL_CONTENT_RENDERER_CLIENT_H_
