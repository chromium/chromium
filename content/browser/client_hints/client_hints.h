// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CLIENT_HINTS_CLIENT_HINTS_H_
#define CONTENT_BROWSER_CLIENT_HINTS_CLIENT_HINTS_H_

#include <memory>
#include <string>

#include "content/common/content_export.h"
#include "content/public/browser/client_hints_controller_delegate.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/parsed_headers.mojom-forward.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "url/gurl.h"

namespace net {
class HttpResponseHeaders;
}  // namespace net

namespace content {

class BrowserContext;
class FrameTreeNode;

// Returns whether client hints can be added for the given URL and frame. This
// is true only if the URL is eligible and JavaScript is enabled.
//
// |origin| is the origin to be used for client hints storage.
// |maybe_request_url| is the url of the request. It is used as an origin for
// the origin trial client hints.
//
// Where possible, |origin| should be the origin of the document the navigation
// is creating. NavigationRequest::GetOriginToCommit() is used, and takes
// sandbox into account, which means that |origin| can be opaque. When called at
// request time, NavigationRequest::GetTentativeOriginAtRequestTime() should be
// used, because it is not possible to determine the origin before receiving the
// final navigation response and the CSP:sandbox response header. It takes into
// account the sandbox flags set by the embedder only.
//
// If |request_url| is not provided, |origin| must not be opaque. This is the
// case for Critical-CH processing and ACCEPT_CH frame processing, where the
// document is not taken into account.
CONTENT_EXPORT bool ShouldAddClientHints(
    const url::Origin& origin,
    FrameTreeNode* frame_tree_node,
    ClientHintsControllerDelegate* delegate,
    const std::optional<GURL> maybe_request_url = std::nullopt);

// Returns |rtt| after adding host-specific random noise, and rounding it as
// per the NetInfo spec to improve privacy.
CONTENT_EXPORT unsigned long RoundRttForTesting(
    const std::string& host,
    const std::optional<base::TimeDelta>& rtt);

// Returns downlink (in Mbps) after adding host-specific random noise to
// |downlink_kbps| (which is in Kbps), and rounding it as per the NetInfo spec
// to improve privacy.
CONTENT_EXPORT double RoundKbpsToMbpsForTesting(
    const std::string& host,
    const std::optional<int32_t>& downlink_kbps);

// Returns true if there is a hint in |critical_hints| that would be sent (i.e.
// not blocked by browser or origin level preferences like disabled JavaScript
// or Feature/Permission Policy) but is not currently in the client hint
// storage.
CONTENT_EXPORT bool AreCriticalHintsMissing(
    const url::Origin& origin,
    FrameTreeNode* frame_tree_node,
    ClientHintsControllerDelegate* delegate,
    const std::vector<network::mojom::WebClientHintsType>& critical_hints);

// Updates the user agent client hint headers. This is called if the value of
// |override_ua| changes after the NavigationRequest was created.
//
// See |ShouldAddClientHints| for |origin| vs |request_url|
CONTENT_EXPORT void UpdateNavigationRequestClientUaHeaders(
    const url::Origin& origin,
    ClientHintsControllerDelegate* delegate,
    bool override_ua,
    FrameTreeNode* frame_tree_node,
    net::HttpRequestHeaders* headers,
    const std::optional<GURL>& request_url = std::nullopt);

CONTENT_EXPORT void AddNavigationRequestClientHintsHeaders(
    const url::Origin& origin,
    net::HttpRequestHeaders* headers,
    BrowserContext* context,
    ClientHintsControllerDelegate* delegate,
    bool is_ua_override_on,
    FrameTreeNode*,
    const blink::ParsedPermissionsPolicy&,
    const std::optional<GURL>& request_url = std::nullopt);

// Adds client hints headers for a prefetch navigation that is not associated
// with a frame. It must be a main frame navigation. |is_javascript_enabled| is
// whether JavaScript is enabled in blink or not.
CONTENT_EXPORT void AddPrefetchNavigationRequestClientHintsHeaders(
    const url::Origin& origin,
    net::HttpRequestHeaders* headers,
    BrowserContext* context,
    ClientHintsControllerDelegate* delegate,
    bool is_ua_override_on,
    bool is_javascript_enabled);

// Parses incoming client hints and persists them as appropriate. Returns
// hints that were accepted as enabled even if they are not going to be
// persisted.
//
// The ParsedHeaders are used to retrieve the already parsed Accept-CH header
// values. The HttpResponseHeaders are not meant to be used by non-sandboxed
// processes, but here, we just pass the HttpRequestHeaders to the
// TrialTokenValidator library.  There is precedent for calling the
// TrialTokenValidator from the browser process, see crrev.com/c/2142580.
CONTENT_EXPORT std::optional<std::vector<network::mojom::WebClientHintsType>>
ParseAndPersistAcceptCHForNavigation(
    const url::Origin& origin,
    const network::mojom::ParsedHeadersPtr& parsed_headers,
    const net::HttpResponseHeaders* response_headers,
    BrowserContext* context,
    ClientHintsControllerDelegate* delegate,
    FrameTreeNode*);

// Persists the `hints` in the Accept-CH storage for the Origin of `url`.
// `delegate` cannot be nullptr.
CONTENT_EXPORT void PersistAcceptCH(
    const url::Origin& origin,
    FrameTreeNode& frame_tree_node,
    ClientHintsControllerDelegate* delegate,
    const std::vector<network::mojom::WebClientHintsType>& hints);

// Looks up which client hints the renderer should be told to enable
// (after subjecting them to permissions policy).
//
// Note that this is based on the top-level frame, and not necessarily the
// frame being committed.
//
// See |ShouldAddClientHints| for |origin| vs |request_url|
CONTENT_EXPORT std::vector<::network::mojom::WebClientHintsType>
LookupAcceptCHForCommit(const url::Origin& origin,
                        ClientHintsControllerDelegate* delegate,
                        FrameTreeNode* frame_tree_node,
                        const std::optional<GURL>& request_url = std::nullopt);

}  // namespace content

#endif  // CONTENT_BROWSER_CLIENT_HINTS_CLIENT_HINTS_H_
