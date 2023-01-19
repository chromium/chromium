// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/webid_utils.h"

#include "base/strings/stringprintf.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/federated_identity_permission_context_delegate.h"
#include "content/public/common/web_identity.h"
#include "net/base/net_errors.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/origin.h"

using blink::mojom::FederatedAuthRequestResult;

namespace content::webid {

void SetIdpSigninStatus(content::BrowserContext* context,
                        const url::Origin& origin,
                        blink::mojom::IdpSigninStatus status) {
  auto* delegate = context->GetFederatedIdentityPermissionContext();
  if (!delegate) {
    // The embedder may not have a delegate (e.g. webview)
    return;
  }
  delegate->SetIdpSigninStatus(
      origin, status == blink::mojom::IdpSigninStatus::kSignedIn);
}

absl::optional<std::string> ComputeConsoleMessageForHttpResponseCode(
    const char* endpoint_name,
    int http_response_code) {
  // Do not add error message for OK response status.
  if (http_response_code >= 200 && http_response_code <= 299)
    return absl::nullopt;

  if (http_response_code < 0) {
    // In this case, the |response_code| represents a NET_ERROR, so we should
    // use a helper function to ensure we use a meaningful message.
    return base::StringPrintf(
        "The fetch of the %s resulted in a network error: %s", endpoint_name,
        net::ErrorToShortString(http_response_code).c_str());
  }
  // In this case, the |response_code| represents an HTTP error code, which is
  // standard and hence the number by itself should be understood.
  return base::StringPrintf(
      "When fetching the %s, a %d HTTP response code was received.",
      endpoint_name, http_response_code);
}

bool IsEndpointUrlValid(const GURL& identity_provider_config_url,
                        const GURL& endpoint_url) {
  return url::Origin::Create(identity_provider_config_url)
      .IsSameOriginWith(endpoint_url);
}

std::string GetConsoleErrorMessageFromResult(
    FederatedAuthRequestResult status) {
  switch (status) {
    case FederatedAuthRequestResult::kShouldEmbargo: {
      return "User declined or dismissed prompt. API exponential cool down "
             "triggered.";
    }
    case FederatedAuthRequestResult::kErrorDisabledInSettings: {
      return "Third-party sign in was disabled in browser Site Settings.";
    }
    case FederatedAuthRequestResult::kErrorTooManyRequests: {
      return "Only one navigator.credentials.get request may be outstanding at "
             "one time.";
    }
    case FederatedAuthRequestResult::kErrorFetchingWellKnownHttpNotFound: {
      return "The provider's FedCM well-known file cannot be found.";
    }
    case FederatedAuthRequestResult::kErrorFetchingWellKnownNoResponse: {
      return "The provider's FedCM well-known file fetch resulted in an "
             "error response code.";
    }
    case FederatedAuthRequestResult::kErrorFetchingWellKnownInvalidResponse: {
      return "Provider's FedCM well-known file is invalid.";
    }
    case FederatedAuthRequestResult::kErrorFetchingWellKnownListEmpty: {
      return "Provider's FedCM well-known file has no config URLs.";
    }
    case FederatedAuthRequestResult::kErrorConfigNotInWellKnown: {
      return "Provider's FedCM config file not listed in its well-known file.";
    }
    case FederatedAuthRequestResult::kErrorWellKnownTooBig: {
      return "Provider's FedCM well-known file contains too many config URLs.";
    }
    case FederatedAuthRequestResult::kErrorFetchingConfigHttpNotFound: {
      return "The provider's FedCM config file cannot be found.";
    }
    case FederatedAuthRequestResult::kErrorFetchingConfigNoResponse: {
      return "The provider's FedCM config file fetch resulted in an "
             "error response code.";
    }
    case FederatedAuthRequestResult::kErrorFetchingConfigInvalidResponse: {
      return "Provider's FedCM config file is invalid.";
    }
    case FederatedAuthRequestResult::kErrorFetchingClientMetadataHttpNotFound: {
      return "The provider's client metadata endpoint cannot be found.";
    }
    case FederatedAuthRequestResult::kErrorFetchingClientMetadataNoResponse: {
      return "The provider's client metadata fetch resulted in an error "
             "response code.";
    }
    case FederatedAuthRequestResult::
        kErrorFetchingClientMetadataInvalidResponse: {
      return "Provider's client metadata is invalid.";
    }
    case FederatedAuthRequestResult::kErrorFetchingAccountsHttpNotFound: {
      return "The provider's accounts list endpoint cannot be found.";
    }
    case FederatedAuthRequestResult::kErrorFetchingAccountsNoResponse: {
      return "The provider's accounts list fetch resulted in an error response "
             "code.";
    }
    case FederatedAuthRequestResult::kErrorFetchingAccountsInvalidResponse: {
      return "Provider's accounts list is invalid. Should have received an "
             "\"accounts\" list, where each account must have at least \"id\", "
             "\"name\", and \"email\".";
    }
    case FederatedAuthRequestResult::kErrorFetchingAccountsListEmpty: {
      return "Provider's accounts list is empty.";
    }
    case FederatedAuthRequestResult::kErrorFetchingIdTokenHttpNotFound: {
      return "The provider's id token endpoint cannot be found.";
    }
    case FederatedAuthRequestResult::kErrorFetchingIdTokenNoResponse: {
      return "The provider's token fetch resulted in an error response "
             "code.";
    }
    case FederatedAuthRequestResult::kErrorFetchingIdTokenInvalidResponse: {
      return "Provider's token is invalid.";
    }
    case FederatedAuthRequestResult::kErrorCanceled: {
      return "The request has been aborted.";
    }
    case FederatedAuthRequestResult::kErrorRpPageNotVisible: {
      return "RP page is not visible.";
    }
    case FederatedAuthRequestResult::kError: {
      return "Error retrieving a token.";
    }
    case FederatedAuthRequestResult::kSuccess: {
      // Should not be called with success, as we should not add a console
      // message for success.
      DCHECK(false);
      return "";
    }
  }
}

}  // namespace content::webid
