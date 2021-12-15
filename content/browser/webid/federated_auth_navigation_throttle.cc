// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "federated_auth_navigation_throttle.h"

#include "base/time/time.h"
#include "content/browser/webid/flags.h"
#include "content/browser/webid/idp_network_request_manager.h"
#include "content/browser/webid/redirect_uri_data.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/federated_identity_request_permission_context_delegate.h"
#include "content/public/browser/federated_identity_sharing_permission_context_delegate.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "ui/base/page_transition_types.h"

namespace content {

namespace {

// Determines if the source and destination would need special permission to
// get access to 1st party state in each other's contexts. Currently this only
// determines if they are same-site based on the public suffix list, but later
// should account for other considerations such as first-party sets or
// enterprise policies.
// Returns true if the source and destination would need permission. Both
// arguments must be absolute URLs.
bool AreCookieIsolatedPrincipals(url::Origin src_origin,
                                 url::Origin dest_origin) {
  if (src_origin.scheme() != dest_origin.scheme()) {
    return true;
  }

  if (!net::registry_controlled_domains::SameDomainOrHost(
          src_origin, dest_origin,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    return true;
  }

  return false;
}

}  // namespace

// static
std::unique_ptr<NavigationThrottle>
FederatedAuthNavigationThrottle::MaybeCreateThrottleFor(
    NavigationHandle* handle) {
  if (!IsFedCmInterceptionEnabled() || handle->GetParentFrameOrOuterDocument())
    return nullptr;

  return std::make_unique<FederatedAuthNavigationThrottle>(handle);
}

FederatedAuthNavigationThrottle::FederatedAuthNavigationThrottle(
    NavigationHandle* handle)
    : NavigationThrottle(handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

FederatedAuthNavigationThrottle::~FederatedAuthNavigationThrottle() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

NavigationThrottle::ThrottleCheckResult
FederatedAuthNavigationThrottle::WillStartRequest() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  GURL navigation_url = navigation_handle()->GetURL();

  // Explicit FedCM requests are exempt from throttling.
  const auto headers = navigation_handle()->GetRequestHeaders();
  if (headers.HasHeader(kSecFedCmCsrfHeader))
    return NavigationThrottle::PROCEED;

  const auto initiator_origin = navigation_handle()->GetInitiatorOrigin();
  url::Origin navigation_origin = url::Origin::Create(navigation_url);

  if (IsFederationRequest(navigation_url) && initiator_origin &&
      AreCookieIsolatedPrincipals(*initiator_origin, navigation_origin)) {
    net::GetValueForKeyInQuery(navigation_url, "redirect_uri", &redirect_uri_);

    // Permission dialog is skipped if this RP/IdP pair already have the
    // Identity Request permission.
    auto* request_permission_delegate =
        navigation_handle()
            ->GetWebContents()
            ->GetBrowserContext()
            ->GetFederatedIdentityRequestPermissionContext();
    if (request_permission_delegate &&
        request_permission_delegate->HasRequestPermission(*initiator_origin,
                                                          navigation_origin)) {
      RedirectUriData::Set(navigation_handle()->GetWebContents(),
                           redirect_uri_);
      return NavigationThrottle::PROCEED;
    }

    request_dialog_controller_ =
        GetContentClient()->browser()->CreateIdentityRequestDialogController();
    request_dialog_controller_->ShowInitialPermissionDialog(
        navigation_handle()->GetWebContents(), navigation_url,
        IdentityRequestDialogController::PermissionDialogMode::kStateless,
        base::BindOnce(&FederatedAuthNavigationThrottle::OnSigninApproved,
                       weak_ptr_factory_.GetWeakPtr()));
    return NavigationThrottle::DEFER;
  } else if (IsFederationResponse(navigation_url)) {
    // TODO(kenrb): Currently no action, this may proceed. Two things to
    // change here:
    //     1) Check the redirect_uri and verify we are going back to the
    //        original source, from which the user consented to login.
    //        Set the session management permission if the IdP wants it.
    //        First, that permission has to be created.
    //        https://crbug.com/1223570.
    //     2) (In the eventual future where directed identifiers are
    //        important) Prompt the user for permission to share personalized
    //        identifiers and store the FEDERATED_IDENTITY_SHARING
    //        setting. https://crbug.com/1141125.
    return NavigationThrottle::PROCEED;
  }

  return NavigationThrottle::PROCEED;
}

bool FederatedAuthNavigationThrottle::IsFederationRequest(GURL url) {
  // Matches OAuth Requests:
  // TODO: make a separation between OpenID Connect and OAuth?
  // TODO: match SAML requests.

  if (!url.has_query()) {
    return false;
  }

  std::string client_id;
  if (!net::GetValueForKeyInQuery(url, "client_id", &client_id)) {
    return false;
  }

  std::string scope;
  if (!net::GetValueForKeyInQuery(url, "scope", &scope)) {
    return false;
  }

  std::string redirect_uri;
  if (!net::GetValueForKeyInQuery(url, "redirect_uri", &redirect_uri)) {
    return false;
  }

  return true;
}

bool FederatedAuthNavigationThrottle::IsFederationResponse(GURL url) {
  // Matches an expected OAuth Response
  if (!RedirectUriData::Get(navigation_handle()->GetWebContents())) {
    return false;
  }
  GURL redirect_url = GURL(
      RedirectUriData::Get(navigation_handle()->GetWebContents())->Value());
  if (url.DeprecatedGetOriginAsURL() ==
          redirect_url.DeprecatedGetOriginAsURL() &&
      url.path() == redirect_url.path()) {
    return true;
  }
  return false;
}

NavigationThrottle::ThrottleCheckResult
FederatedAuthNavigationThrottle::WillRedirectRequest() {
  return WillStartRequest();
}

void FederatedAuthNavigationThrottle::OnSigninApproved(
    IdentityRequestDialogController::UserApproval approval) {
  if (approval == IdentityRequestDialogController::UserApproval::kApproved) {
    auto* request_permission_delegate =
        navigation_handle()
            ->GetWebContents()
            ->GetBrowserContext()
            ->GetFederatedIdentityRequestPermissionContext();
    const auto initiator_origin = navigation_handle()->GetInitiatorOrigin();
    if (request_permission_delegate && initiator_origin) {
      request_permission_delegate->GrantRequestPermission(
          *initiator_origin,
          url::Origin::Create(navigation_handle()->GetURL()));
    }
    RedirectUriData::Set(navigation_handle()->GetWebContents(), redirect_uri_);
    Resume();
    return;
  }

  CancelDeferredNavigation(NavigationThrottle::CANCEL);
}

void FederatedAuthNavigationThrottle::OnTokenProvisionApproved(
    IdentityRequestDialogController::UserApproval approval) {
  if (approval == IdentityRequestDialogController::UserApproval::kApproved) {
    auto* sharing_permission_delegate =
        navigation_handle()
            ->GetWebContents()
            ->GetBrowserContext()
            ->GetFederatedIdentitySharingPermissionContext();
    const auto initiator_origin = navigation_handle()->GetInitiatorOrigin();
    if (sharing_permission_delegate && initiator_origin) {
      sharing_permission_delegate->GrantSharingPermission(
          *initiator_origin,
          url::Origin::Create(navigation_handle()->GetURL()));
    }
    Resume();
    return;
  }
  CancelDeferredNavigation(NavigationThrottle::CANCEL);
}

const char* FederatedAuthNavigationThrottle::GetNameForLogging() {
  return "FederatedAuthNavigationThrottle";
}

}  // namespace content
