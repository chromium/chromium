// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_URL_LOADER_FACTORY_MANAGER_H_
#define EXTENSIONS_BROWSER_URL_LOADER_FACTORY_MANAGER_H_

#include "base/macros.h"
#include "content/public/browser/navigation_handle.h"
#include "extensions/common/extension.h"
#include "extensions/common/host_id.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "url/gurl.h"

namespace content {
class RenderFrameHost;
class RenderProcessHost;
}  // namespace content

namespace url {
class Origin;
}  // namespace url

namespace extensions {

class URLLoaderFactoryManagerBrowserTest;

// This class manages URLLoaderFactory objects that handle network requests that
// require extension-specific permissions (related to relaxed CORB and CORS).
//
// See also https://crbug.com/846346 for motivation for having separate
// URLLoaderFactory objects for content scripts.
class URLLoaderFactoryManager {
 public:
  // Only static methods.
  URLLoaderFactoryManager() = delete;

  // To be called before a navigation commits (to ensure that the renderer gets
  // the special URLLoaderFactory before injecting content scripts declared in
  // an extension manifest).
  //
  // This method will inspect all enabled extensions and ask RenderFrameHost to
  // create separate URLLoaderFactory objects for the extensions that declare in
  // their manifest desire to inject content scripts into the target of the
  // |navigation|.
  static void ReadyToCommitNavigation(content::NavigationHandle* navigation);

  // To be called before ExtensionMsg_ExecuteCode is sent to a renderer process
  // (to ensure that the renderer gets the special URLLoaderFactory before
  // injecting content script requested via chrome.tabs.executeScript).
  //
  // This method may ask RenderFrameHost to create a separate URLLoaderFactory
  // object for extension identified by |host_id|.  The caller needs to ensure
  // that if |host_id.type() == HostID::EXTENSIONS|, then the extension with the
  // given id exists and is enabled.
  static void WillExecuteCode(content::RenderFrameHost* frame,
                              const HostID& host_id);

  // Creates a URLLoaderFactory that should be used for requests initiated from
  // |process| by |origin|.
  //
  // The return value depends on the intended consumer of the URLLoaderFactory.
  // - "web": "null" PendingRemote is returned to indicate that an
  //   extensions-agnostic, default URLLoaderFactory should be used
  // - "extension": URLLoaderFactory with extension-specific permissions is
  //   returned for extension frames (e.g. to be used from an extension
  //   background page).
  // - "content script": "null" PendingRemote is returned for most extensions
  //   (non-allowlisted, manifest V3+), but some extensions might need a
  //   separate URLLoaderFactory for content scripts (with extension-specific
  //   permissions).
  // The method recognizes the intended consumer based on |origin| ("web" vs
  // other cases) and |process| ("extension" vs "content script").
  //
  // If a new factory is returned, then its security properties (e.g.
  // |is_corb_enabled| and/or |factory_bound_access_patterns|) will match
  // the requirements of the extension associated with the |origin|.
  //
  // |main_world_origin| is typically the same as |origin|, except if the
  // returned factory will be used by a content script).  |main_world_origin|
  // will be used (if needed) as the value of |request_initiator_site_lock|.
  // The following examples might help understand the difference between
  // |origin| and |main_world_origin|:
  //
  //                               |    web      |  extension  | content script
  // ------------------------------|-------------|-------------|---------------
  // network::ResourceRequest:     |             |             |
  // - request_initiator           |    web      |  extension  |    web
  // - isolated_world_origin       |   nullopt   |   nullopt   |  extension
  //                               |             |             |
  // CreateFactory method params:  |             |             |
  // - origin                      |    web      |  extension  |  extension
  // - main_world_origin           |    web      |  extension  |    web
  //                               |             |             |
  // Returned URLLoaderFactory     |   null      | new factory | new factory if
  //                               |             |             |  allowlisted
  // URLLoaderFactoryParams of the |             |             |
  // returned or default factory:  |             |             |
  // - request_initiator_site_lock |    web      |  extension  |    web
  // - is_corb_enabled, etc.       |  secure     |  ext-based  | ext-based if
  //                               |    default  |             |   allowlisted
  static mojo::PendingRemote<network::mojom::URLLoaderFactory> CreateFactory(
      content::RenderProcessHost* process,
      network::mojom::NetworkContext* network_context,
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
          header_client,
      const url::Origin& origin,
      const url::Origin& main_world_origin,
      const base::Optional<net::NetworkIsolationKey>& network_isolation_key);

  static void AddExtensionToAllowlistForTesting(const Extension& extension);
  static void RemoveExtensionFromAllowlistForTesting(
      const Extension& extension);

 private:
  // If |extension|'s manifest declares that it may inject JavaScript content
  // script into the |navigating_frame| / |navigation_target|, then
  // DoContentScriptsMatchNavigation returns true.  Otherwise it may return
  // either true or false.  Note that this method ignores CSS content scripts.
  static bool DoContentScriptsMatchNavigatingFrame(
      const Extension& extension,
      content::RenderFrameHost* navigating_frame,
      const GURL& navigation_target);
  friend class URLLoaderFactoryManagerBrowserTest;

  DISALLOW_COPY_AND_ASSIGN(URLLoaderFactoryManager);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_URL_LOADER_FACTORY_MANAGER_H_
