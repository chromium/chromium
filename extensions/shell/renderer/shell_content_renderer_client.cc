// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/renderer/shell_content_renderer_client.h"

#include <memory>

#include "components/nacl/common/buildflags.h"
#include "content/public/common/content_constants.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/common/extensions_client.h"
#include "extensions/renderer/api/core_extensions_renderer_api_provider.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/shell/common/shell_extensions_client.h"
#include "extensions/shell/renderer/api/shell_extensions_renderer_api_provider.h"
#include "extensions/shell/renderer/shell_extensions_renderer_client.h"
#include "third_party/blink/public/web/web_local_frame.h"

#if BUILDFLAG(ENABLE_NACL)
#include "components/nacl/common/nacl_constants.h"
#include "components/nacl/renderer/nacl_helper.h"
#endif

using blink::WebFrame;
using blink::WebString;
using content::RenderThread;

namespace extensions {

ShellContentRendererClient::ShellContentRendererClient() = default;
ShellContentRendererClient::~ShellContentRendererClient() = default;

void ShellContentRendererClient::RenderThreadStarted() {
  extensions_client_.reset(CreateExtensionsClient());
  ExtensionsClient::Set(extensions_client_.get());

  extensions_renderer_client_ =
      std::make_unique<ShellExtensionsRendererClient>();
  extensions_renderer_client_->AddAPIProvider(
      std::make_unique<CoreExtensionsRendererAPIProvider>());
  extensions_renderer_client_->AddAPIProvider(
      std::make_unique<ShellExtensionsRendererAPIProvider>());
  ExtensionsRendererClient::Set(extensions_renderer_client_.get());
  extensions_renderer_client_->RenderThreadStarted();
}

void ShellContentRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame) {
  Dispatcher* dispatcher = extensions_renderer_client_->dispatcher();
  // ExtensionFrameHelper destroys itself when the RenderFrame is destroyed.
  new ExtensionFrameHelper(render_frame, dispatcher);

  dispatcher->OnRenderFrameCreated(render_frame);

  // TODO(jamescook): Do we need to add a new PepperHelper(render_frame) here?
  // It doesn't seem necessary for either Pepper or NaCl.
  // http://crbug.com/403004
#if BUILDFLAG(ENABLE_NACL)
  new nacl::NaClHelper(render_frame);
#endif
}

bool ShellContentRendererClient::OverrideCreatePlugin(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params,
    blink::WebPlugin** plugin) {
  // Allow the content module to create the plugin.
  return false;
}

blink::WebPlugin* ShellContentRendererClient::CreatePluginReplacement(
    content::RenderFrame* render_frame,
    const base::FilePath& plugin_path) {
  // Don't provide a custom "failed to load" plugin.
  return nullptr;
}

void ShellContentRendererClient::WillSendRequest(
    blink::WebLocalFrame* frame,
    ui::PageTransition transition_type,
    const blink::WebURL& upstream_url,
    const blink::WebURL& target_url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin* initiator_origin,
    GURL* new_url) {
  // TODO(jamescook): Cause an error for bad extension scheme requests?
}

bool ShellContentRendererClient::IsExternalPepperPlugin(
    const std::string& module_name) {
#if BUILDFLAG(ENABLE_NACL)
  // TODO(bbudge) remove this when the trusted NaCl plugin has been removed.
  // We must defer certain plugin events for NaCl instances since we switch
  // from the in-process to the out-of-process proxy after instantiating them.
  return module_name == nacl::kNaClPluginName;
#else
  return false;
#endif
}

void ShellContentRendererClient::RunScriptsAtDocumentStart(
    content::RenderFrame* render_frame) {
  extensions_renderer_client_->dispatcher()->RunScriptsAtDocumentStart(
      render_frame);
}

void ShellContentRendererClient::RunScriptsAtDocumentEnd(
    content::RenderFrame* render_frame) {
  extensions_renderer_client_->dispatcher()->RunScriptsAtDocumentEnd(
      render_frame);
}

void ShellContentRendererClient::SetClientsForTesting(
    std::unique_ptr<ExtensionsClient> extensions_client,
    std::unique_ptr<ShellExtensionsRendererClient> extensions_renderer_client) {
  DCHECK(!extensions_client_);
  extensions_client_ = std::move(extensions_client);
  ExtensionsClient::Set(extensions_client_.get());

  DCHECK(!extensions_renderer_client_);
  extensions_renderer_client_ = std::move(extensions_renderer_client);
  ExtensionsRendererClient::Set(extensions_renderer_client_.get());
}

ExtensionsClient* ShellContentRendererClient::CreateExtensionsClient() {
  return new ShellExtensionsClient;
}

}  // namespace extensions
