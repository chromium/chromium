// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_RENDERER_SHELL_CONTENT_RENDERER_CLIENT_H_
#define EXTENSIONS_SHELL_RENDERER_SHELL_CONTENT_RENDERER_CLIENT_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "content/public/renderer/content_renderer_client.h"

namespace blink {
class WebURL;
}  // namespace blink

namespace extensions {

class ExtensionsClient;
class ExtensionsGuestViewContainerDispatcher;
class ShellExtensionsRendererClient;

// Renderer initialization and runtime support for app_shell.
class ShellContentRendererClient : public content::ContentRendererClient {
 public:
  ShellContentRendererClient();
  ~ShellContentRendererClient() override;

  // content::ContentRendererClient implementation:
  void RenderThreadStarted() override;
  void RenderFrameCreated(content::RenderFrame* render_frame) override;
  bool OverrideCreatePlugin(content::RenderFrame* render_frame,
                            const blink::WebPluginParams& params,
                            blink::WebPlugin** plugin) override;
  blink::WebPlugin* CreatePluginReplacement(
      content::RenderFrame* render_frame,
      const base::FilePath& plugin_path) override;
  void WillSendRequest(blink::WebLocalFrame* frame,
                       ui::PageTransition transition_type,
                       const blink::WebURL& url,
                       const net::SiteForCookies& site_for_cookies,
                       const url::Origin* initiator_origin,
                       GURL* new_url,
                       bool* force_ignore_site_for_cookies) override;
  bool IsExternalPepperPlugin(const std::string& module_name) override;
  void RunScriptsAtDocumentStart(content::RenderFrame* render_frame) override;
  void RunScriptsAtDocumentEnd(content::RenderFrame* render_frame) override;

 protected:
  // app_shell embedders may need custom extensions client interfaces.
  // This class takes ownership of the returned object.
  virtual ExtensionsClient* CreateExtensionsClient();

 private:
  std::unique_ptr<ExtensionsClient> extensions_client_;
  std::unique_ptr<ShellExtensionsRendererClient> extensions_renderer_client_;
  std::unique_ptr<ExtensionsGuestViewContainerDispatcher>
      guest_view_container_dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(ShellContentRendererClient);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_RENDERER_SHELL_CONTENT_RENDERER_CLIENT_H_
