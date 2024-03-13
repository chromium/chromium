// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EXTENSIONS_RENDERER_CLIENT_H_
#define EXTENSIONS_RENDERER_EXTENSIONS_RENDERER_CLIENT_H_

#include <memory>

#include "extensions/common/extension_id.h"
#include "extensions/renderer/extensions_renderer_api_provider.h"

class GURL;

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

  // Notifies that the main render thread has started. Used to finalize any
  // setup. Mirrors the ContentRendererClient method of the same name.
  virtual void RenderThreadStarted() = 0;

  // Returns true if the current render process was launched incognito.
  virtual bool IsIncognitoProcess() const = 0;

  // Returns the lowest isolated world ID available to extensions.
  // Must be greater than 0. See blink::WebFrame::executeScriptInIsolatedWorld
  // (third_party/WebKit/public/web/WebFrame.h) for additional context.
  virtual int GetLowestIsolatedWorldId() const = 0;

  // Returns the associated Dispatcher.
  virtual Dispatcher* GetDispatcher() = 0;

  // Notifies the client when an extension is added or removed.
  // TODO(devlin): Make a RendererExtensionRegistryObserver?
  virtual void OnExtensionLoaded(const Extension& extension) {}
  virtual void OnExtensionUnloaded(const ExtensionId& extension) {}

  // Returns whether or not extension APIs are allowed for the specified
  // script. The script must be specified in the extension's manifest
  // background section and the scope must be the root scope of the
  // extension.
  virtual bool ExtensionAPIEnabledForServiceWorkerScript(
      const GURL& scope,
      const GURL& script_url) const = 0;

  // Adds an API provider that can extend the behavior of extension's renderer
  // side. API providers need to be added before |Dispatcher| is created.
  void AddAPIProvider(
      std::unique_ptr<ExtensionsRendererAPIProvider> api_provider);

  // Returns the single instance of |this|.
  static ExtensionsRendererClient* Get();

  // Initialize the single instance.
  static void Set(ExtensionsRendererClient* client);

 protected:
  std::vector<std::unique_ptr<const ExtensionsRendererAPIProvider>>
      api_providers_;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_EXTENSIONS_RENDERER_CLIENT_H_
