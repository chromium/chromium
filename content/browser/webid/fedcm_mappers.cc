// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/fedcm_mappers.h"

#include <string>
#include <vector>

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/fedcm_metrics.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"

using blink::mojom::FederatedAuthRequestResult;
using blink::mojom::RequestTokenStatus;
using ParseStatus = content::IdpNetworkRequestManager::ParseStatus;
using LifecycleStateImpl = content::RenderFrameHostImpl::LifecycleStateImpl;

namespace content {

std::vector<std::string> DisclosureFieldsToStringList(
    const std::vector<IdentityRequestDialogDisclosureField>& fields) {
  std::vector<std::string> list;
  for (auto field : fields) {
    switch (field) {
      case IdentityRequestDialogDisclosureField::kName:
        list.push_back(kFedCmDefaultFieldName);
        break;
      case IdentityRequestDialogDisclosureField::kEmail:
        list.push_back(kFedCmDefaultFieldEmail);
        break;
      case IdentityRequestDialogDisclosureField::kPicture:
        list.push_back(kFedCmDefaultFieldPicture);
        break;
      case IdentityRequestDialogDisclosureField::kPhoneNumber:
        list.push_back(kFedCmFieldPhoneNumber);
        break;
      case IdentityRequestDialogDisclosureField::kUsername:
        list.push_back(kFedCmFieldUsername);
        break;
    }
  }
  return list;
}

RequestTokenStatus FederatedAuthRequestResultToRequestTokenStatus(
    FederatedAuthRequestResult result) {
  // Avoids exposing to renderer detailed error messages which may leak cross
  // site information to the API call site.
  switch (result) {
    case FederatedAuthRequestResult::kSuccess: {
      return RequestTokenStatus::kSuccess;
    }
    case FederatedAuthRequestResult::kTooManyRequests: {
      return RequestTokenStatus::kErrorTooManyRequests;
    }
    case FederatedAuthRequestResult::kCanceled: {
      return RequestTokenStatus::kErrorCanceled;
    }
    case FederatedAuthRequestResult::kShouldEmbargo:
    case FederatedAuthRequestResult::kIdpNotPotentiallyTrustworthy:
    case FederatedAuthRequestResult::kDisabledInSettings:
    case FederatedAuthRequestResult::kDisabledInFlags:
    case FederatedAuthRequestResult::kWellKnownHttpNotFound:
    case FederatedAuthRequestResult::kWellKnownNoResponse:
    case FederatedAuthRequestResult::kWellKnownInvalidResponse:
    case FederatedAuthRequestResult::kWellKnownListEmpty:
    case FederatedAuthRequestResult::kWellKnownInvalidContentType:
    case FederatedAuthRequestResult::kConfigNotInWellKnown:
    case FederatedAuthRequestResult::kWellKnownTooBig:
    case FederatedAuthRequestResult::kConfigHttpNotFound:
    case FederatedAuthRequestResult::kConfigNoResponse:
    case FederatedAuthRequestResult::kConfigInvalidResponse:
    case FederatedAuthRequestResult::kConfigInvalidContentType:
    case FederatedAuthRequestResult::kClientMetadataHttpNotFound:
    case FederatedAuthRequestResult::kClientMetadataNoResponse:
    case FederatedAuthRequestResult::kClientMetadataInvalidResponse:
    case FederatedAuthRequestResult::kClientMetadataInvalidContentType:
    case FederatedAuthRequestResult::kAccountsHttpNotFound:
    case FederatedAuthRequestResult::kAccountsNoResponse:
    case FederatedAuthRequestResult::kAccountsInvalidResponse:
    case FederatedAuthRequestResult::kAccountsListEmpty:
    case FederatedAuthRequestResult::kAccountsInvalidContentType:
    case FederatedAuthRequestResult::kIdTokenHttpNotFound:
    case FederatedAuthRequestResult::kIdTokenNoResponse:
    case FederatedAuthRequestResult::kIdTokenInvalidResponse:
    case FederatedAuthRequestResult::kIdTokenIdpErrorResponse:
    case FederatedAuthRequestResult::kIdTokenCrossSiteIdpErrorResponse:
    case FederatedAuthRequestResult::kIdTokenInvalidContentType:
    case FederatedAuthRequestResult::kRpPageNotVisible:
    case FederatedAuthRequestResult::kSilentMediationFailure:
    case FederatedAuthRequestResult::kThirdPartyCookiesBlocked:
    case FederatedAuthRequestResult::kNotSignedInWithIdp:
    case FederatedAuthRequestResult::kMissingTransientUserActivation:
    case FederatedAuthRequestResult::kReplacedByActiveMode:
    case FederatedAuthRequestResult::kInvalidFieldsSpecified:
    case FederatedAuthRequestResult::kRelyingPartyOriginIsOpaque:
    case FederatedAuthRequestResult::kTypeNotMatching:
    case FederatedAuthRequestResult::kUiDismissedNoEmbargo:
    case FederatedAuthRequestResult::kCorsError:
    case FederatedAuthRequestResult::kSuppressedBySegmentationPlatform:
    case FederatedAuthRequestResult::kError: {
      return RequestTokenStatus::kError;
    }
  }
}

MetricsEndpointErrorCode FederatedAuthRequestResultToMetricsEndpointErrorCode(
    blink::mojom::FederatedAuthRequestResult result) {
  switch (result) {
    case FederatedAuthRequestResult::kSuccess: {
      return MetricsEndpointErrorCode::kNone;
    }
    case FederatedAuthRequestResult::kTooManyRequests:
    case FederatedAuthRequestResult::kMissingTransientUserActivation:
    case FederatedAuthRequestResult::kRelyingPartyOriginIsOpaque:
    case FederatedAuthRequestResult::kInvalidFieldsSpecified:
    case FederatedAuthRequestResult::kCanceled: {
      return MetricsEndpointErrorCode::kRpFailure;
    }
    case FederatedAuthRequestResult::kAccountsInvalidResponse:
    case FederatedAuthRequestResult::kAccountsListEmpty:
    case FederatedAuthRequestResult::kAccountsInvalidContentType: {
      return MetricsEndpointErrorCode::kAccountsEndpointInvalidResponse;
    }
    case FederatedAuthRequestResult::kIdTokenInvalidResponse:
    case FederatedAuthRequestResult::kIdTokenIdpErrorResponse:
    case FederatedAuthRequestResult::kIdTokenCrossSiteIdpErrorResponse:
    case FederatedAuthRequestResult::kIdTokenInvalidContentType:
    case FederatedAuthRequestResult::kCorsError: {
      return MetricsEndpointErrorCode::kTokenEndpointInvalidResponse;
    }
    case FederatedAuthRequestResult::kShouldEmbargo:
    case FederatedAuthRequestResult::kUiDismissedNoEmbargo:
    case FederatedAuthRequestResult::kDisabledInFlags:
    case FederatedAuthRequestResult::kDisabledInSettings:
    case FederatedAuthRequestResult::kThirdPartyCookiesBlocked:
    case FederatedAuthRequestResult::kRpPageNotVisible:
    case FederatedAuthRequestResult::kReplacedByActiveMode:
    case FederatedAuthRequestResult::kNotSignedInWithIdp: {
      return MetricsEndpointErrorCode::kUserFailure;
    }
    case FederatedAuthRequestResult::kWellKnownHttpNotFound:
    case FederatedAuthRequestResult::kWellKnownNoResponse:
    case FederatedAuthRequestResult::kConfigHttpNotFound:
    case FederatedAuthRequestResult::kConfigNoResponse:
    case FederatedAuthRequestResult::kClientMetadataHttpNotFound:
    case FederatedAuthRequestResult::kClientMetadataNoResponse:
    case FederatedAuthRequestResult::kAccountsHttpNotFound:
    case FederatedAuthRequestResult::kAccountsNoResponse:
    case FederatedAuthRequestResult::kIdTokenHttpNotFound:
    case FederatedAuthRequestResult::kIdTokenNoResponse: {
      return MetricsEndpointErrorCode::kIdpServerUnavailable;
    }
    case FederatedAuthRequestResult::kConfigNotInWellKnown:
    case FederatedAuthRequestResult::kWellKnownTooBig: {
      return MetricsEndpointErrorCode::kManifestError;
    }
    case FederatedAuthRequestResult::kWellKnownListEmpty:
    case FederatedAuthRequestResult::kWellKnownInvalidResponse:
    case FederatedAuthRequestResult::kConfigInvalidResponse:
    case FederatedAuthRequestResult::kClientMetadataInvalidResponse:
    case FederatedAuthRequestResult::kWellKnownInvalidContentType:
    case FederatedAuthRequestResult::kConfigInvalidContentType:
    case FederatedAuthRequestResult::kClientMetadataInvalidContentType: {
      return MetricsEndpointErrorCode::kIdpServerInvalidResponse;
    }
    case FederatedAuthRequestResult::kIdpNotPotentiallyTrustworthy:
    case FederatedAuthRequestResult::kError:
    case FederatedAuthRequestResult::kSilentMediationFailure:
    case FederatedAuthRequestResult::kTypeNotMatching:
    case FederatedAuthRequestResult::kSuppressedBySegmentationPlatform: {
      return MetricsEndpointErrorCode::kOther;
    }
  }
}

std::pair<FederatedAuthRequestResult, FedCmRequestIdTokenStatus>
AccountParseStatusToRequestResultAndTokenStatus(ParseStatus parse_status) {
  switch (parse_status) {
    case ParseStatus::kHttpNotFoundError: {
      return {FederatedAuthRequestResult::kAccountsHttpNotFound,
              FedCmRequestIdTokenStatus::kAccountsHttpNotFound};
    }
    case ParseStatus::kNoResponseError: {
      return {FederatedAuthRequestResult::kAccountsNoResponse,
              FedCmRequestIdTokenStatus::kAccountsNoResponse};
    }
    case ParseStatus::kInvalidResponseError: {
      return {FederatedAuthRequestResult::kAccountsInvalidResponse,
              FedCmRequestIdTokenStatus::kAccountsInvalidResponse};
    }
    case ParseStatus::kEmptyListError: {
      return {FederatedAuthRequestResult::kAccountsListEmpty,
              FedCmRequestIdTokenStatus::kAccountsListEmpty};
    }
    case ParseStatus::kInvalidContentTypeError: {
      return {FederatedAuthRequestResult::kAccountsInvalidContentType,
              FedCmRequestIdTokenStatus::kAccountsInvalidContentType};
    }
    case ParseStatus::kSuccess: {
      NOTREACHED() << "Should not be invoked on success";
    }
  }
}

FedCmLifecycleStateFailureReason
LifecycleStateImplLifecycleStateImplToFedCmLifecycleStateFailureReason(
    LifecycleStateImpl lifecycle_state) {
  switch (lifecycle_state) {
    case LifecycleStateImpl::kSpeculative: {
      return FedCmLifecycleStateFailureReason::kSpeculative;
    }
    case LifecycleStateImpl::kPendingCommit: {
      return FedCmLifecycleStateFailureReason::kPendingCommit;
    }
    case LifecycleStateImpl::kPrerendering: {
      return FedCmLifecycleStateFailureReason::kPrerendering;
    }
    case LifecycleStateImpl::kInBackForwardCache: {
      return FedCmLifecycleStateFailureReason::kInBackForwardCache;
    }
    case LifecycleStateImpl::kRunningUnloadHandlers: {
      return FedCmLifecycleStateFailureReason::kRunningUnloadHandlers;
    }
    case LifecycleStateImpl::kReadyToBeDeleted: {
      return FedCmLifecycleStateFailureReason::kReadyToBeDeleted;
    }
    default: {
      return FedCmLifecycleStateFailureReason::kOther;
    }
  }
}

}  // namespace content
