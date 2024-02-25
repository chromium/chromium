// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_URL_LOADER_FACTORY_MANAGER_H_
#define EXTENSIONS_BROWSER_URL_LOADER_FACTORY_MANAGER_H_

#include "base/types/pass_key.h"
#include "content/public/browser/navigation_handle.h"
#include "extensions/common/extension.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

namespace extensions {

class ScriptInjectionTracker;

// This class manages URLLoaderFactory objects that handle network requests that
// require extension-specific permissions (related to relaxed ORB and CORS).
//
// See also https://crbug.com/846346 for motivation for having separate
// URLLoaderFactory objects for content scripts.
class URLLoaderFactoryManager {
 public:
  // Only static methods.
  URLLoaderFactoryManager() = delete;
  URLLoaderFactoryManager(const URLLoaderFactoryManager&) = delete;
  URLLoaderFactoryManager& operator=(const URLLoaderFactoryManager&) = delete;

  // Invoked when `navigation` is ready to commit with the set of `extensions`
  // asked to inject content script into the target frame using
  // declarations in the extension manifest approach:
  // https://developer.chrome.com/docs/extensions/mv2/content_scripts/#declaratively
  static void WillInjectContentScriptsWhenNavigationCommits(
      base::PassKey<ScriptInjectionTracker> pass_key,
      content::NavigationHandle* navigation,
      const std::vector<const Extension*>& extensions);

  // Invoked when `extension` asks to inject a content script into `frame`
  // (invoked before an IPC with the content script injection request is
  // actually sent to the renderer process).  This covers injections via
  // `chrome.declarativeContent` and `chrome.scripting.executeScript` APIs -
  // see:
  // https://developer.chrome.com/docs/extensions/mv2/content_scripts/#programmatic
  // and
  // https://developer.chrome.com/docs/extensions/reference/declarativeContent/#type-RequestContentScript
  static void WillProgrammaticallyInjectContentScript(
      base::PassKey<ScriptInjectionTracker> pass_key,
      content::RenderFrameHost* frame,
      const Extension& extension);

  // Creates a URLLoaderFactory that should be used for requests initiated from
  // |process| by |origin|.
  //
  // The behavior of this method depends on the intended consumer of the
  // URLLoaderFactory:
  // - "web": No changes are made to |factory_params| - an extensions-agnostic,
  //   default URLLoaderFactory should be used
  // - "extension": Extension-specific permissions are set in |factory_params|
  //   if the factory will be used by an extension frame (e.g. from an extension
  //   background page).
  // - "content script": For most extensions no changes are made to
  //   |factory_params|, but platform apps might need to set app-specific
  //   security properties in the URLLoaderFactory used by content scripts.
  // The method recognizes the intended consumer based on |origin| ("web" vs
  // other cases) and |is_for_isolated_world| ("extension" vs "content script").
  //
  // The following examples might help understand the difference between
  // |origin| and other properties of a factory and/or network request:
  //
  //                                 |   web     |  extension  | content script
  // --------------------------------|-----------|-------------|---------------
  // network::ResourceRequest:       |           |             |
  // - request_initiator             |    web    |  extension  |     web
  // - isolated_world_origin         |  nullopt  |   nullopt   |  extension
  //                                 |           |             |
  // OverrideFactory...Params:       |           |             |
  // - origin                        |    web    |  extension  |  extension
  //                                 |           |             |
  // URLLoaderFactoryParams:         |           |             |
  // - request_initiator_origin_lock |    web    |  extension  |     web
  // - overridden properties?        |    no     |     yes     |  if needed
  //    - is_orb_enabled             | secure-   |  ext-based  | ext-based for
  //    - ..._access_patterns        |  -default |             | platform apps
  static void OverrideURLLoaderFactoryParams(
      content::BrowserContext* browser_context,
      const url::Origin& origin,
      bool is_for_isolated_world,
      network::mojom::URLLoaderFactoryParams* factory_params);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_URL_LOADER_FACTORY_MANAGER_H_
