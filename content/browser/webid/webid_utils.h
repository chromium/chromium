// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_WEBID_UTILS_H_
#define CONTENT_BROWSER_WEBID_WEBID_UTILS_H_

#include <optional>

#include "content/browser/webid/idp_network_request_manager.h"
#include "content/common/content_export.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace blink::mojom {
enum class FederatedAuthRequestResult;
enum class IdpSigninStatus;
}  // namespace blink::mojom

namespace content {
class BrowserContext;
enum class FedCmDisconnectStatus;
enum class FedCmIdpSigninStatusMode;
class FederatedIdentityApiPermissionContextDelegate;
class FederatedIdentityPermissionContextDelegate;
enum class IdpSigninStatus;
class FederatedAuthRequestPageData;

namespace webid {

// Returns true if `origin` is same site with `render_frame_host` and
// all its ancestors. Also returns true if there are no ancestors or
// if `render_frame_host` is null.
bool IsSameSiteWithAncestors(const url::Origin& origin,
                             RenderFrameHost* render_frame_host);

void SetIdpSigninStatus(BrowserContext* context,
                        FrameTreeNodeId frame_tree_node_id,
                        const url::Origin& origin,
                        blink::mojom::IdpSigninStatus status);

// Computes string to display in developer tools console for a FedCM endpoint
// request with the passed-in `endpoint_name` and which returns the passed-in
// `http_response_code`. Returns std::nullopt if the `http_response_code` does
// not represent an error in the fetch.
std::optional<std::string> ComputeConsoleMessageForHttpResponseCode(
    const char* endpoint_name,
    int http_response_code);

// Returns whether a FedCM endpoint URL is valid given the passed-in config
// endpoint URL.
bool IsEndpointSameOrigin(const GURL& identity_provider_config_url,
                          const GURL& endpoint_url);

// Returns whether the two origins are considered same-site (same eTLD+1). Also
// ensures that the scheme is the same.
bool IsSameSite(const url::Origin& origin1, const url::Origin& origin2);

// Returns whether FedCM should fail/skip the accounts endpoint request because
// the user is not signed-in to the IdP.
bool ShouldFailAccountsEndpointRequestBecauseNotSignedInWithIdp(
    RenderFrameHost& host,
    const GURL& identity_provider_config_url,
    FederatedIdentityPermissionContextDelegate* permission_delegate);

// Updates the IdP sign-in status based on the accounts endpoint response. Also
// logs IdP sign-in status related UMA metrics.
//
// `does_idp_have_failing_idp_signin_status` indicates whether the accounts
// endpoint request would have been failed/skipped had the IdP signin-status
// been FedCmIdpSigninStatusMode::ENABLED.
void UpdateIdpSigninStatusForAccountsEndpointResponse(
    RenderFrameHost& host,
    const GURL& identity_provider_config_url,
    IdpNetworkRequestManager::FetchStatus account_endpoint_fetch_status,
    bool does_idp_have_failing_idp_signin_status,
    FederatedIdentityPermissionContextDelegate* permission_delegate);

// Returns a string to be used as the console error message from a
// FederatedAuthRequestResult.
CONTENT_EXPORT std::string GetConsoleErrorMessageFromResult(
    blink::mojom::FederatedAuthRequestResult result);

// Returns a string to be used as the console error message for a disconnect()
// call.
CONTENT_EXPORT std::string GetDisconnectConsoleErrorMessage(
    FedCmDisconnectStatus disconnect_status_for_metrics);

FedCmIdpSigninStatusMode GetIdpSigninStatusMode(RenderFrameHost& host,
                                                const url::Origin& idp_origin);

// Returns the eTLD+1 for a given url. For localhost, returns the host.
std::string FormatUrlForDisplay(const GURL& url);

// Returns true if the user has used FedCM to login to the RP via the IdP
// account or if the IdP has third party cookies access. For the former, if
// |account| is provided, we look for the specific account. Otherwise we look
// for *any* account.
bool HasSharingPermissionOrIdpHasThirdPartyCookiesAccess(
    RenderFrameHost& host,
    const GURL& provider_url,
    const url::Origin& embedder_origin,
    const url::Origin& requester_origin,
    const std::optional<std::string>& account_id,
    FederatedIdentityPermissionContextDelegate* sharing_permission_delegate,
    FederatedIdentityApiPermissionContextDelegate* api_permission_delegate);

bool IsFedCmAuthzEnabled(RenderFrameHost& host, const url::Origin& idp_origin);

FederatedAuthRequestPageData* GetPageData(Page& page);

// Returns a new session ID. Used to record UKM metrics corresponding to a new
// API invocation, like get() or disconnect().
int GetNewSessionID();
}  // namespace webid

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_WEBID_UTILS_H_
