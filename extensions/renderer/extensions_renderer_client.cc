// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/renderer/extensions_renderer_client.h"

#include <memory>
#include <ostream>

#include "base/check.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/metrics/histogram_functions.h"
#include "content/public/common/content_constants.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_set.h"
#include "extensions/common/mojom/context_type.mojom.h"
#include "extensions/common/switches.h"
#include "extensions/renderer/dispatcher.h"
#include "extensions/renderer/extension_frame_helper.h"
#include "extensions/renderer/extension_web_view_helper.h"
#include "extensions/renderer/extensions_render_frame_observer.h"
#include "extensions/renderer/renderer_extension_registry.h"
#include "extensions/renderer/resource_request_policy.h"
#include "extensions/renderer/script_context.h"
#include "third_party/blink/public/common/security/protocol_handler_security_level.h"
#include "third_party/blink/public/platform/web_url.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_plugin_params.h"

#if BUILDFLAG(ENABLE_GUEST_VIEW)
#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_container_manager.h"
#endif

namespace extensions {

namespace {

ExtensionsRendererClient* g_client = nullptr;

#if BUILDFLAG(ENABLE_GUEST_VIEW)
void IsGuestViewApiAvailableToScriptContext(bool* api_is_available,
                                            ScriptContext* context) {
  if (context->GetAvailability("guestViewInternal").is_available()) {
    *api_is_available = true;
  }
}
#endif

}  // namespace

ExtensionsRendererClient::ExtensionsRendererClient() = default;
ExtensionsRendererClient::~ExtensionsRendererClient() = default;

ExtensionsRendererClient* ExtensionsRendererClient::Get() {
  CHECK(g_client);
  return g_client;
}

void ExtensionsRendererClient::Set(ExtensionsRendererClient* client) {
  g_client = client;
}

void ExtensionsRendererClient::OnExtensionLoaded(const Extension& extension) {
  resource_request_policy_->OnExtensionLoaded(extension);
}

void ExtensionsRendererClient::OnExtensionUnloaded(
    const ExtensionId& extension_id) {
  resource_request_policy_->OnExtensionUnloaded(extension_id);
}

void ExtensionsRendererClient::AddAPIProvider(
    std::unique_ptr<ExtensionsRendererAPIProvider> api_provider) {
  CHECK(!dispatcher_)
      << "API providers must be added before the Dispatcher is instantiated.";
  api_providers_.push_back(std::move(api_provider));
}

void ExtensionsRendererClient::RenderThreadStarted() {
  // Tests may create their own ExtensionDispatcher and inject it using
  // `SetDispatcherForTesting()`. Don't overwrite it.
  if (!dispatcher()) {
    dispatcher_ = std::make_unique<Dispatcher>(std::move(api_providers_));
  }
  content::RenderThread* thread = content::RenderThread::Get();
  dispatcher()->OnRenderThreadStarted(thread);
  thread->AddObserver(dispatcher());

  resource_request_policy_ = std::make_unique<ResourceRequestPolicy>(
      dispatcher(), CreateResourceRequestPolicyDelegate());

  FinishInitialization();
}

void ExtensionsRendererClient::WebViewCreated(
    blink::WebView* web_view,
    const url::Origin* outermost_origin) {
  new ExtensionWebViewHelper(web_view, outermost_origin);
}

void ExtensionsRendererClient::RenderFrameCreated(
    content::RenderFrame* render_frame,
    service_manager::BinderRegistry* registry) {
  new ExtensionsRenderFrameObserver(render_frame, registry);
  new ExtensionFrameHelper(render_frame, dispatcher());
  dispatcher_->OnRenderFrameCreated(render_frame);
}

bool ExtensionsRendererClient::OverrideCreatePlugin(
    content::RenderFrame* render_frame,
    const blink::WebPluginParams& params) {
#if BUILDFLAG(ENABLE_GUEST_VIEW)
  if (params.mime_type.Utf8() != content::kBrowserPluginMimeType) {
    return true;
  }

  bool guest_view_api_available = false;
  dispatcher_->script_context_set_iterator()->ForEach(
      render_frame, base::BindRepeating(&IsGuestViewApiAvailableToScriptContext,
                                        &guest_view_api_available));
  return !guest_view_api_available;
#else
  return true;
#endif
}

bool ExtensionsRendererClient::AllowPopup() {
  ScriptContext* current_context =
      dispatcher_->script_context_set().GetCurrent();
  if (!current_context || !current_context->extension()) {
    return false;
  }

  // See http://crbug.com/117446 for the subtlety of this check.
  switch (current_context->context_type()) {
    case mojom::ContextType::kUnspecified:
    case mojom::ContextType::kWebPage:
    case mojom::ContextType::kUnprivilegedExtension:
    case mojom::ContextType::kWebUi:
    case mojom::ContextType::kUntrustedWebUi:
    case mojom::ContextType::kOffscreenExtension:
    case mojom::ContextType::kUserScript:
    case mojom::ContextType::kLockscreenExtension:
      return false;
    case mojom::ContextType::kPrivilegedExtension:
      return !current_context->IsForServiceWorker();
    case mojom::ContextType::kContentScript:
      return true;
    case mojom::ContextType::kPrivilegedWebPage:
      return current_context->web_frame()->IsOutermostMainFrame();
  }
}

blink::ProtocolHandlerSecurityLevel
ExtensionsRendererClient::GetProtocolHandlerSecurityLevel() {
  // WARNING: This must match the logic of
  // Browser::GetProtocolHandlerSecurityLevel().
  ScriptContext* current_context =
      dispatcher_->script_context_set().GetCurrent();
  if (!current_context || !current_context->extension()) {
    return blink::ProtocolHandlerSecurityLevel::kStrict;
  }

  switch (current_context->context_type()) {
    case mojom::ContextType::kPrivilegedWebPage:
    case mojom::ContextType::kContentScript:
    case mojom::ContextType::kLockscreenExtension:
    case mojom::ContextType::kOffscreenExtension:
    case mojom::ContextType::kUnprivilegedExtension:
    case mojom::ContextType::kUnspecified:
    case mojom::ContextType::kUserScript:
    case mojom::ContextType::kWebUi:
    case mojom::ContextType::kUntrustedWebUi:
    case mojom::ContextType::kWebPage:
      return blink::ProtocolHandlerSecurityLevel::kStrict;
    case mojom::ContextType::kPrivilegedExtension:
      return blink::ProtocolHandlerSecurityLevel::kExtensionFeatures;
  }
}

v8::Local<v8::Object> ExtensionsRendererClient::GetScriptableObject(
    const blink::WebElement& plugin_element,
    v8::Isolate* isolate) {
#if BUILDFLAG(ENABLE_GUEST_VIEW)
  // If there is a MimeHandlerView that can provide the scriptable object then
  // MaybeCreateMimeHandlerView must have been called before and a container
  // manager should exist.
  auto* container_manager = MimeHandlerViewContainerManager::Get(
      content::RenderFrame::FromWebFrame(
          plugin_element.GetDocument().GetFrame()),
      false /* create_if_does_not_exist */);
  if (container_manager) {
    return container_manager->GetScriptableObject(plugin_element, isolate);
  }
#endif
  return v8::Local<v8::Object>();
}

// static
blink::WebFrame* ExtensionsRendererClient::FindFrame(
    blink::WebLocalFrame* relative_to_frame,
    const std::string& name) {
  content::RenderFrame* result = ExtensionFrameHelper::FindFrame(
      content::RenderFrame::FromWebFrame(relative_to_frame), name);
  return result ? result->GetWebFrame() : nullptr;
}

void ExtensionsRendererClient::RunScriptsAtDocumentStart(
    content::RenderFrame* render_frame) {
  dispatcher_->RunScriptsAtDocumentStart(render_frame);
}

void ExtensionsRendererClient::RunScriptsAtDocumentEnd(
    content::RenderFrame* render_frame) {
  dispatcher_->RunScriptsAtDocumentEnd(render_frame);
}

void ExtensionsRendererClient::RunScriptsAtDocumentIdle(
    content::RenderFrame* render_frame) {
  dispatcher_->RunScriptsAtDocumentIdle(render_frame);
}

void ExtensionsRendererClient::WillSendRequest(
    blink::WebLocalFrame* frame,
    ui::PageTransition transition_type,
    const blink::WebURL& upstream_url,
    const blink::WebURL& target_url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin* initiator_origin,
    GURL* new_url) {
  std::string extension_id;
  if (initiator_origin &&
      initiator_origin->scheme() == extensions::kExtensionScheme) {
    extension_id = initiator_origin->host();
  } else {
    if (site_for_cookies.scheme() == extensions::kExtensionScheme) {
      extension_id = site_for_cookies.registrable_domain();
    }
  }

  if (!extension_id.empty()) {
    const extensions::RendererExtensionRegistry* extension_registry =
        extensions::RendererExtensionRegistry::Get();
    const Extension* extension = extension_registry->GetByID(extension_id);
    if (!extension) {
      // If there is no extension installed for the origin, it may be from a
      // recently uninstalled extension.  The tabs of such extensions are
      // automatically closed, but subframes and content scripts may stick
      // around. Fail such requests without killing the process.
      *new_url = GURL(kExtensionInvalidRequestURL);
    }
  }

  // The rest of this method is only concerned with extensions URLs.
  if (!target_url.ProtocolIs(extensions::kExtensionScheme)) {
    return;
  }

  if (target_url.ProtocolIs(extensions::kExtensionScheme) &&
      !resource_request_policy_->CanRequestResource(
          upstream_url, target_url, frame, transition_type, initiator_origin)) {
    *new_url = GURL(kExtensionInvalidRequestURL);
  }

  RecordMetricsForURLRequest(frame, target_url);
}

void ExtensionsRendererClient::SetDispatcherForTesting(
    std::unique_ptr<Dispatcher> dispatcher) {
  dispatcher_ = std::move(dispatcher);
}

std::unique_ptr<ResourceRequestPolicy::Delegate>
ExtensionsRendererClient::CreateResourceRequestPolicyDelegate() {
  return nullptr;
}

}  // namespace extensions
