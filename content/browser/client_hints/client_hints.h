// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CLIENT_HINTS_CLIENT_HINTS_H_
#define CONTENT_BROWSER_CLIENT_HINTS_CLIENT_HINTS_H_

#include <memory>
#include <string>

#include "content/public/browser/client_hints_controller_delegate.h"
#include "net/http/http_request_headers.h"
#include "services/network/public/mojom/parsed_headers.mojom-forward.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"

namespace content {

class BrowserContext;
class FrameTreeNode;

// Returns whether client hints can be added for the given URL and frame. This
// is true only if the URL is eligible and JavaScript is enabled.
CONTENT_EXPORT bool ShouldAddClientHints(
    const GURL& url,
    FrameTreeNode* frame_tree_node,
    ClientHintsControllerDelegate* delegate);

// Returns |rtt| after adding host-specific random noise, and rounding it as
// per the NetInfo spec to improve privacy.
CONTENT_EXPORT unsigned long RoundRttForTesting(
    const std::string& host,
    const base::Optional<base::TimeDelta>& rtt);

// Returns downlink (in Mbps) after adding host-specific random noise to
// |downlink_kbps| (which is in Kbps), and rounding it as per the NetInfo spec
// to improve privacy.
CONTENT_EXPORT double RoundKbpsToMbpsForTesting(
    const std::string& host,
    const base::Optional<int32_t>& downlink_kbps);

// Returns true if there is a hint in |critical_hints| that would be sent (i.e.
// not blocked by browser or origin level preferences like disabled JavaScript
// or Feature/Permission Policy) but is not currently in the client hint
// storage.
CONTENT_EXPORT bool AreCriticalHintsMissing(
    const GURL& url,
    FrameTreeNode* frame_tree_node,
    ClientHintsControllerDelegate* delegate,
    const std::vector<network::mojom::WebClientHintsType>& critical_hints);

// Updates the user agent client hint headers. This is called if the value of
// |override_ua| changes after the NavigationRequest was created.
CONTENT_EXPORT void UpdateNavigationRequestClientUaHeaders(
    const GURL& url,
    ClientHintsControllerDelegate* delegate,
    bool override_ua,
    FrameTreeNode* frame_tree_node,
    net::HttpRequestHeaders* headers);

CONTENT_EXPORT void AddNavigationRequestClientHintsHeaders(
    const GURL& url,
    net::HttpRequestHeaders* headers,
    BrowserContext* context,
    ClientHintsControllerDelegate* delegate,
    bool is_ua_override_on,
    FrameTreeNode*,
    const blink::ParsedPermissionsPolicy&);

// Adds client hints headers for a prefetch navigation that is not associated
// with a frame. It must be a main frame navigation. |is_javascript_enabled| is
// whether JavaScript is enabled in blink or not.
CONTENT_EXPORT void AddPrefetchNavigationRequestClientHintsHeaders(
    const GURL& url,
    net::HttpRequestHeaders* headers,
    BrowserContext* context,
    ClientHintsControllerDelegate* delegate,
    bool is_ua_override_on,
    bool is_javascript_enabled);

// Parses incoming client hints and persists them as appropriate. Returns
// hints that were accepted as enabled even if they are not going to be
// persisted. The distinction is relevant in legacy case where permissions
// policy is off and there is no valid Accept-CH-Lifetime, where the header
// still applies locally within frame.
CONTENT_EXPORT base::Optional<std::vector<network::mojom::WebClientHintsType>>
ParseAndPersistAcceptCHForNagivation(
    const GURL& url,
    const ::network::mojom::ParsedHeadersPtr& headers,
    BrowserContext* context,
    ClientHintsControllerDelegate* delegate,
    FrameTreeNode*);

// Looks up which client hints the renderer should be told to enable
// (after subjecting them to permissions policy).
//
// Note that this is based on the top-level frame, and not necessarily the
// frame being committed.
CONTENT_EXPORT std::vector<::network::mojom::WebClientHintsType>
LookupAcceptCHForCommit(const GURL& url,
                        ClientHintsControllerDelegate* delegate,
                        FrameTreeNode* frame_tree_node);

}  // namespace content

#endif  // CONTENT_BROWSER_CLIENT_HINTS_CLIENT_HINTS_H_
