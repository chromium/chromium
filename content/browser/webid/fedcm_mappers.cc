// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/fedcm_mappers.h"

#include <string>
#include <vector>

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/fedcm_metrics.h"
#include "content/browser/webid/flags.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-forward.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom-forward.h"

using blink::mojom::FederatedAuthRequestResult;
using blink::mojom::RequestTokenStatus;
using ErrorDialogResult = content::FedCmErrorDialogResult;
using ParseStatus = content::IdpNetworkRequestManager::ParseStatus;
using LifecycleStateImpl = content::RenderFrameHostImpl::LifecycleStateImpl;
using FederatedApiPermissionStatus =
    content::FederatedIdentityApiPermissionContextDelegate::PermissionStatus;

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
    case ParseStatus::kHttpNotFoundError:
      return {FederatedAuthRequestResult::kAccountsHttpNotFound,
              FedCmRequestIdTokenStatus::kAccountsHttpNotFound};
    case ParseStatus::kNoResponseError:
      return {FederatedAuthRequestResult::kAccountsNoResponse,
              FedCmRequestIdTokenStatus::kAccountsNoResponse};
    case ParseStatus::kInvalidResponseError:
      return {FederatedAuthRequestResult::kAccountsInvalidResponse,
              FedCmRequestIdTokenStatus::kAccountsInvalidResponse};
    case ParseStatus::kEmptyListError:
      return {FederatedAuthRequestResult::kAccountsListEmpty,
              FedCmRequestIdTokenStatus::kAccountsListEmpty};
    case ParseStatus::kInvalidContentTypeError:
      return {FederatedAuthRequestResult::kAccountsInvalidContentType,
              FedCmRequestIdTokenStatus::kAccountsInvalidContentType};
    case ParseStatus::kSuccess:
      NOTREACHED() << "Should not be invoked on success";
  }
}

FedCmLifecycleStateFailureReason
LifecycleStateImplLifecycleStateImplToFedCmLifecycleStateFailureReason(
    LifecycleStateImpl lifecycle_state) {
  switch (lifecycle_state) {
    case LifecycleStateImpl::kSpeculative:
      return FedCmLifecycleStateFailureReason::kSpeculative;
    case LifecycleStateImpl::kPendingCommit:
      return FedCmLifecycleStateFailureReason::kPendingCommit;
    case LifecycleStateImpl::kPrerendering:
      return FedCmLifecycleStateFailureReason::kPrerendering;
    case LifecycleStateImpl::kInBackForwardCache:
      return FedCmLifecycleStateFailureReason::kInBackForwardCache;
    case LifecycleStateImpl::kRunningUnloadHandlers:
      return FedCmLifecycleStateFailureReason::kRunningUnloadHandlers;
    case LifecycleStateImpl::kReadyToBeDeleted:
      return FedCmLifecycleStateFailureReason::kReadyToBeDeleted;
    default:
      return FedCmLifecycleStateFailureReason::kOther;
  }
}

std::pair<FederatedAuthRequestResult, FedCmRequestIdTokenStatus>
PermissionStatusToRequestResultAndTokenStatus(
    content::FederatedIdentityApiPermissionContextDelegate::PermissionStatus
        permission_status) {
  switch (permission_status) {
    case FederatedApiPermissionStatus::BLOCKED_VARIATIONS:
      return {FederatedAuthRequestResult::kDisabledInFlags,
              FedCmRequestIdTokenStatus::kDisabledInFlags};
    case FederatedApiPermissionStatus::BLOCKED_SETTINGS:
      return {FederatedAuthRequestResult::kDisabledInSettings,
              FedCmRequestIdTokenStatus::kDisabledInSettings};
    case FederatedApiPermissionStatus::BLOCKED_EMBARGO:
      return {FederatedAuthRequestResult::kDisabledInSettings,
              FedCmRequestIdTokenStatus::kDisabledEmbargo};
    case FederatedApiPermissionStatus::GRANTED:
      NOTREACHED() << "Should not be invoked with GRANTED";
  }
}

FedCmErrorDialogResult DismissReasonToErrorDialogResult(
    IdentityRequestDialogController::DismissReason dismiss_reason,
    bool has_url) {
  switch (dismiss_reason) {
    case IdentityRequestDialogController::DismissReason::kCloseButton:
      return has_url ? ErrorDialogResult::kCloseWithMoreDetails
                     : ErrorDialogResult::kCloseWithoutMoreDetails;
    case IdentityRequestDialogController::DismissReason::kSwipe:
      return has_url ? ErrorDialogResult::kSwipeWithMoreDetails
                     : ErrorDialogResult::kSwipeWithoutMoreDetails;
    case IdentityRequestDialogController::DismissReason::kGotItButton:
      return has_url ? ErrorDialogResult::kGotItWithMoreDetails
                     : ErrorDialogResult::kGotItWithoutMoreDetails;
    case IdentityRequestDialogController::DismissReason::kMoreDetailsButton:
      return ErrorDialogResult::kMoreDetails;
    default:
      return has_url ? ErrorDialogResult::kOtherWithMoreDetails
                     : ErrorDialogResult::kOtherWithoutMoreDetails;
  }
}

std::pair<FederatedAuthRequestResult, FedCmRequestIdTokenStatus>
IdAssertionFetchStatusToRequestResultAndTokenStatus(
    IdpNetworkRequestManager::FetchStatus status) {
  switch (status.parse_status) {
    case ParseStatus::kHttpNotFoundError:
      return {FederatedAuthRequestResult::kIdTokenHttpNotFound,
              FedCmRequestIdTokenStatus::kIdTokenHttpNotFound};
    case ParseStatus::kNoResponseError: {
      if (status.cors_error) {
        return {FederatedAuthRequestResult::kCorsError,
                FedCmRequestIdTokenStatus::kIdTokenNoResponse};
      }
      return {FederatedAuthRequestResult::kIdTokenNoResponse,
              FedCmRequestIdTokenStatus::kIdTokenNoResponse};
    }
    case ParseStatus::kInvalidResponseError:
      return {FederatedAuthRequestResult::kIdTokenInvalidResponse,
              FedCmRequestIdTokenStatus::kIdTokenInvalidResponse};
    case ParseStatus::kInvalidContentTypeError:
      return {FederatedAuthRequestResult::kIdTokenInvalidContentType,
              FedCmRequestIdTokenStatus::kIdTokenInvalidContentType};
    case ParseStatus::kEmptyListError:
      NOTREACHED() << "EmptyListError is not an option for this fetch";
    case IdpNetworkRequestManager::ParseStatus::kSuccess:
      NOTREACHED() << "Should not be invoked with success";
  }
}

std::vector<IdentityRequestDialogDisclosureField> GetDisclosureFields(
    const std::optional<std::vector<std::string>>& fields) {
  const std::vector<IdentityRequestDialogDisclosureField> kDefaultPermissions =
      {IdentityRequestDialogDisclosureField::kName,
       IdentityRequestDialogDisclosureField::kEmail,
       IdentityRequestDialogDisclosureField::kPicture};

  if (!fields) {
    // If "fields" is not passed, defaults the parameter to
    // ["name", "email" and "picture"].
    return kDefaultPermissions;
  }

  // If fields is explicitly empty, we should not mediate.
  if (fields->empty()) {
    return {};
  }

  std::vector<IdentityRequestDialogDisclosureField> list;
  for (const auto& field : *fields) {
    if (field == kFedCmDefaultFieldName) {
      list.push_back(IdentityRequestDialogDisclosureField::kName);
    } else if (field == kFedCmDefaultFieldEmail) {
      list.push_back(IdentityRequestDialogDisclosureField::kEmail);
    } else if (field == kFedCmDefaultFieldPicture) {
      list.push_back(IdentityRequestDialogDisclosureField::kPicture);
    } else if (IsFedCmAlternativeIdentifiersEnabled()) {
      if (field == kFedCmFieldPhoneNumber) {
        list.push_back(IdentityRequestDialogDisclosureField::kPhoneNumber);
      } else if (field == kFedCmFieldUsername) {
        list.push_back(IdentityRequestDialogDisclosureField::kUsername);
      }
    }
  }
  return list;
}

}  // namespace content
