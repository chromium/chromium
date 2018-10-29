// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_URL_LOADER_FACTORY_MANAGER_H_
#define EXTENSIONS_BROWSER_URL_LOADER_FACTORY_MANAGER_H_

#include "base/macros.h"
#include "content/public/browser/navigation_handle.h"
#include "extensions/common/extension.h"
#include "extensions/common/host_id.h"
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
  // |process| by |initiator_origin|.  Returns a "null" InterfacePtrInfo if the
  // default, extensions-agnostic URLLoaderFactory should be used (if either
  // |initiator_origin| is not associated with an extension, or the extension
  // doesn't need a special URLLoaderFactory).
  static network::mojom::URLLoaderFactoryPtrInfo CreateFactory(
      content::RenderProcessHost* process,
      network::mojom::NetworkContext* network_context,
      const url::Origin& initiator_origin);

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
