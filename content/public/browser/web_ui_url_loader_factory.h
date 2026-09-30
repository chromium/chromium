// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_UI_URL_LOADER_FACTORY_H_
#define CONTENT_PUBLIC_BROWSER_WEB_UI_URL_LOADER_FACTORY_H_

#include <optional>
#include <string>

#include "base/containers/flat_set.h"
#include "base/gtest_prod_util.h"
#include "base/types/pass_key.h"
#include "content/common/content_export.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "services/network/public/mojom/url_loader_factory.mojom-forward.h"
#include "url/origin.h"

class ChromeContentBrowserClient;
class DevToolsUIBindings;

namespace chromecast::shell {
class CastContentBrowserClient;
}

namespace extensions {
class WebUIURLFetcher;
}

namespace content {

class BrowserContext;
class DownloadManagerImpl;
class NavigationURLLoaderImpl;
class RenderFrameHost;
class SaveFileManager;

// Passkey restricting creation of WebUI URLLoaderFactories without an origin
// lock (unconstrained mode) to explicitly allowlisted browser-internal callers.
//
// A factory without an origin lock skips the cross-origin checks that
// WebUIURLLoaderFactory otherwise enforces (see CreateWebUIURLLoaderFactory()),
// so it can read from any data source within its scheme and allowed hosts. If
// handed to a renderer, a compromised renderer could use it to read data it
// shouldn't have access to.
//
// Avoid adding new callers; prefer passing an origin lock. If one is really
// needed, it should either be browser-initiated and never handed to a
// renderer, or be restricted via |allowed_hosts| to data sources that are safe
// to expose to that renderer.
class CONTENT_EXPORT WebUIURLLoaderFactoryPasskey {
 public:
  using PassKey = base::PassKey<WebUIURLLoaderFactoryPasskey>;

 private:
  static PassKey GetPassKey() { return PassKey(); }

  // Allowlisted browser-process callers that load resources without a specific
  // renderer initiator origin lock (e.g. navigations, downloads, DevTools,
  // extensions).
  friend class DownloadManagerImpl;
  friend class NavigationURLLoaderImpl;
  friend class SaveFileManager;
  friend class chromecast::shell::CastContentBrowserClient;
  friend class ::ChromeContentBrowserClient;
  friend class ::DevToolsUIBindings;
  friend class extensions::WebUIURLFetcher;

  // Tests.
  friend class WebUIURLLoaderFactoryOriginLockTest;
  friend class WebUIURLLoaderFactoryTest;
  friend class WebUIURLLoaderFactoryTest_RangeRequest_Test;
  friend class WebUIURLLoaderFactoryInvalidUrlTest;
  friend class WebUIURLLoaderFactoryInvalidUrlTest_InvalidUrl_Test;
  FRIEND_TEST_ALL_PREFIXES(WebUIURLLoaderFactoryOriginLockTest,
                           NoInitiatorOriginLockAllowsAllRequests);
  FRIEND_TEST_ALL_PREFIXES(WebUIURLLoaderFactoryTest, RangeRequest);
  FRIEND_TEST_ALL_PREFIXES(WebUIURLLoaderFactoryInvalidUrlTest, InvalidUrl);
  FRIEND_TEST_ALL_PREFIXES(WebUIURLLoaderFactoryInvalidUrlTest,
                           DevToolsSchemeRejectsChromeDataSource);
};

// Returns a URLLoaderFactory that can load resources from the given WebUI
// scheme and allowed hosts (e.g. "chrome", "chrome-untrusted"). If
// |allowed_hosts| is empty, all hosts in the scheme are allowed.
//
// |request_initiator_origin_lock| is the origin of the document or worker when
// handed out to a renderer process for subresource loading.
// Because WebUIURLLoaderFactory is a non-network factory, it enforces cross-
// origin access control directly against this browser-verified origin lock
// rather than renderer-provided request headers, preventing a compromised
// renderer from spoofing its initiator.
// Requests from a chrome-untrusted:// initiator to a different origin are then
// only served when the target URLDataSource allows that initiator through
// URLDataSource::GetAccessControlAllowOriginForOrigin().
CONTENT_EXPORT
mojo::PendingRemote<network::mojom::URLLoaderFactory>
CreateWebUIURLLoaderFactory(RenderFrameHost* render_frame_host,
                            const std::string& scheme,
                            base::flat_set<std::string> allowed_hosts,
                            const url::Origin& request_initiator_origin_lock);

// Similar to the above method, but used for worker processes.
CONTENT_EXPORT
mojo::PendingRemote<network::mojom::URLLoaderFactory>
CreateWebUIURLLoaderFactoryForWorker(
    BrowserContext* browser_context,
    const std::string& scheme,
    base::flat_set<std::string> allowed_hosts,
    const url::Origin& request_initiator_origin_lock);

// Creates a WebUIURLLoaderFactory without an origin lock, allowing any resource
// within the configured scheme and allowed hosts to be loaded without
// cross-origin restrictions. This mode is restricted via PassKey to trusted
// browser-process-internal callers (e.g. navigations, downloads, DevTools).
CONTENT_EXPORT
mojo::PendingRemote<network::mojom::URLLoaderFactory>
CreateWebUIURLLoaderFactoryWithoutOriginLock(
    WebUIURLLoaderFactoryPasskey::PassKey pass_key,
    RenderFrameHost* render_frame_host,
    const std::string& scheme,
    base::flat_set<std::string> allowed_hosts);
}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_UI_URL_LOADER_FACTORY_H_
