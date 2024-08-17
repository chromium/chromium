// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EXTENSIONS_RENDERER_CLIENT_H_
#define EXTENSIONS_RENDERER_EXTENSIONS_RENDERER_CLIENT_H_

#include <memory>
#include <string>

#include "extensions/common/extension_id.h"
#include "extensions/renderer/extensions_renderer_api_provider.h"
#include "extensions/renderer/resource_request_policy.h"
#include "services/service_manager/public/cpp/binder_registry.h"
#include "ui/base/page_transition_types.h"
#include "v8/include/v8-local-handle.h"

namespace content {
class RenderFrame;
}  // namespace content

namespace blink {
enum class ProtocolHandlerSecurityLevel;
class WebElement;
class WebFrame;
class WebLocalFrame;
struct WebPluginParams;
class WebURL;
class WebView;
}  // namespace blink

namespace net {
class SiteForCookies;
}

namespace url {
class Origin;
}

namespace v8 {
class Isolate;
class Object;
}  // namespace v8

namespace extensions {
class Extension;
class ExtensionsRendererAPIProvider;
class Dispatcher;

// Interface to allow the extensions module to make render-process-specific
// queries of the embedder. Should be Set() once in the render process.
//
// NOTE: Methods that do not require knowledge of renderer concepts should be
// added in ExtensionsClient (extensions/common/extensions_client.h) even if
// they are only used in the renderer process.
class ExtensionsRendererClient {
 public:
  ExtensionsRendererClient();
  virtual ~ExtensionsRendererClient();

  // Returns true if the current render process was launched incognito.
  virtual bool IsIncognitoProcess() const = 0;

  // Returns the lowest isolated world ID available to extensions.
  // Must be greater than 0. See blink::WebFrame::executeScriptInIsolatedWorld
  // (third_party/WebKit/public/web/WebFrame.h) for additional context.
  virtual int GetLowestIsolatedWorldId() const = 0;

  // Notifies the client when an extension is added or removed.
  // TODO(devlin): Make a RendererExtensionRegistryObserver?
  void OnExtensionLoaded(const Extension& extension);
  void OnExtensionUnloaded(const ExtensionId& extension);

  // Adds an API provider that can extend the behavior of extension's renderer
  // side. API providers need to be added before |Dispatcher| is created.
  void AddAPIProvider(
      std::unique_ptr<ExtensionsRendererAPIProvider> api_provider);

  // The following methods mirror the ContentRendererClient methods of the same
  // names.
  void RenderThreadStarted();
  void WebViewCreated(blink::WebView* web_view,
                      const url::Origin* outermost_origin);
  void RenderFrameCreated(content::RenderFrame* render_frame,
                          service_manager::BinderRegistry* registry);
  bool OverrideCreatePlugin(content::RenderFrame* render_frame,
                            const blink::WebPluginParams& params);
  bool AllowPopup();
  blink::ProtocolHandlerSecurityLevel GetProtocolHandlerSecurityLevel();
  v8::Local<v8::Object> GetScriptableObject(
      const blink::WebElement& plugin_element,
      v8::Isolate* isolate);
  static blink::WebFrame* FindFrame(blink::WebLocalFrame* relative_to_frame,
                                    const std::string& name);
  void RunScriptsAtDocumentStart(content::RenderFrame* render_frame);
  void RunScriptsAtDocumentEnd(content::RenderFrame* render_frame);
  void RunScriptsAtDocumentIdle(content::RenderFrame* render_frame);
  void WillSendRequest(blink::WebLocalFrame* frame,
                       ui::PageTransition transition_type,
                       const blink::WebURL& upstream_url,
                       const blink::WebURL& target_url,
                       const net::SiteForCookies& site_for_cookies,
                       const url::Origin* initiator_origin,
                       GURL* new_url);

  // Returns the single instance of |this|.
  static ExtensionsRendererClient* Get();

  // Initialize the single instance.
  static void Set(ExtensionsRendererClient* client);

  Dispatcher* dispatcher() { return dispatcher_.get(); }

  void SetDispatcherForTesting(std::unique_ptr<Dispatcher> dispatcher);

 private:
  // Called to allow embedders to finish any initialization as part of
  // RenderThreadStarted().
  virtual void FinishInitialization() {}

  // Allows embedders to create a delegate for the ResourceRequestPolicy.
  // By default, returns null.
  virtual std::unique_ptr<ResourceRequestPolicy::Delegate>
  CreateResourceRequestPolicyDelegate();

  // Allows embedders to record metrics when a request is being sent to
  // `target_url`.
  virtual void RecordMetricsForURLRequest(blink::WebLocalFrame* frame,
                                          const blink::WebURL& target_url) {}

  std::vector<std::unique_ptr<const ExtensionsRendererAPIProvider>>
      api_providers_;

  std::unique_ptr<Dispatcher> dispatcher_;
  std::unique_ptr<ResourceRequestPolicy> resource_request_policy_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_EXTENSIONS_RENDERER_CLIENT_H_
