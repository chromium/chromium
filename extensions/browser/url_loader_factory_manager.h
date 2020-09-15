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
#include "url/gurl.h"

namespace content {
class BrowserContext;
class RenderFrameHost;
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
  // The behavior of this method depends on the intended consumer of the
  // URLLoaderFactory:
  // - "web": No changes are made to |factory_params| - an extensions-agnostic,
  //   default URLLoaderFactory should be used
  // - "extension": Extension-specific permissions are set in |factory_params|
  //   if the factory will be used by an extension frame (e.g. from an extension
  //   background page).
  // - "content script": For most extensions no changes are made to
  //   |factory_params| (e.g. for non-allowlisted and/or manifest V3+
  //   extensions), but some extensions might need to set extension-specific
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
  // - overridden properties?        |   no      |     yes     |  if needed
  //    - is_corb_enabled            | secure    |  ext-based  | ext-based if
  //    - ..._access_patterns        |   default |             |   allowlisted
  static void OverrideURLLoaderFactoryParams(
      content::BrowserContext* browser_context,
      const url::Origin& origin,
      bool is_for_isolated_world,
      network::mojom::URLLoaderFactoryParams* factory_params);

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
