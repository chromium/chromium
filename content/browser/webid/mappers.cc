// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/mappers.h"

#include <string>
#include <vector>

#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/delegation/email_verification_request.h"
#include "content/browser/webid/flags.h"
#include "content/browser/webid/metrics.h"
#include "content/public/browser/webid/identity_credential_source.h"
#include "content/public/browser/webid/identity_request_dialog_controller.h"
#include "third_party/blink/public/mojom/devtools/inspector_issue.mojom-forward.h"
#include "third_party/blink/public/mojom/webid/federated_request.mojom-forward.h"

namespace content::webid {

using FederatedApiPermissionStatus =
    FederatedIdentityApiPermissionContextDelegate::PermissionStatus;
using LifecycleStateImpl = RenderFrameHostImpl::LifecycleStateImpl;
using blink::mojom::EmailVerificationRequestResult;
using blink::mojom::FederatedRequestResult;
using blink::mojom::RequestTokenStatus;

std::vector<std::string> DisclosureFieldsToStringList(
    const std::vector<IdentityRequestDialogDisclosureField>& fields) {
  std::vector<std::string> list;
  for (auto field : fields) {
    switch (field) {
      case IdentityRequestDialogDisclosureField::kName:
        list.push_back(kDefaultFieldName);
        break;
      case IdentityRequestDialogDisclosureField::kEmail:
        list.push_back(kDefaultFieldEmail);
        break;
      case IdentityRequestDialogDisclosureField::kPicture:
        list.push_back(kDefaultFieldPicture);
        break;
      case IdentityRequestDialogDisclosureField::kPhoneNumber:
        list.push_back(kFieldPhoneNumber);
        break;
      case IdentityRequestDialogDisclosureField::kUsername:
        list.push_back(kFieldUsername);
        break;
    }
  }
  return list;
}

RequestTokenStatus FederatedRequestResultToRequestTokenStatus(
    FederatedRequestResult result) {
  // Avoids exposing to renderer detailed error messages which may leak cross
  // site information to the API call site.
  switch (result) {
    case FederatedRequestResult::kSuccess: {
      return RequestTokenStatus::kSuccess;
    }
    case FederatedRequestResult::kTooManyRequests: {
      return RequestTokenStatus::kErrorTooManyRequests;
    }
    case FederatedRequestResult::kCanceled: {
      return RequestTokenStatus::kErrorCanceled;
    }
    case FederatedRequestResult::kShouldEmbargo:
    case FederatedRequestResult::kIdpNotPotentiallyTrustworthy:
    case FederatedRequestResult::kDisabledInSettings:
    case FederatedRequestResult::kDisabledInFlags:
    case FederatedRequestResult::kWellKnownHttpNotFound:
    case FederatedRequestResult::kWellKnownNoResponse:
    case FederatedRequestResult::kWellKnownInvalidResponse:
    case FederatedRequestResult::kWellKnownListEmpty:
    case FederatedRequestResult::kWellKnownInvalidContentType:
    case FederatedRequestResult::kConfigNotInWellKnown:
    case FederatedRequestResult::kWellKnownTooBig:
    case FederatedRequestResult::kConfigHttpNotFound:
    case FederatedRequestResult::kConfigNoResponse:
    case FederatedRequestResult::kConfigInvalidResponse:
    case FederatedRequestResult::kConfigInvalidContentType:
    case FederatedRequestResult::kAccountsHttpNotFound:
    case FederatedRequestResult::kAccountsNoResponse:
    case FederatedRequestResult::kAccountsInvalidResponse:
    case FederatedRequestResult::kAccountsListEmpty:
    case FederatedRequestResult::kAccountsInvalidContentType:
    case FederatedRequestResult::kIdTokenHttpNotFound:
    case FederatedRequestResult::kIdTokenNoResponse:
    case FederatedRequestResult::kIdTokenInvalidResponse:
    case FederatedRequestResult::kIdTokenIdpErrorResponse:
    case FederatedRequestResult::kIdTokenCrossSiteIdpErrorResponse:
    case FederatedRequestResult::kIdTokenInvalidContentType:
    case FederatedRequestResult::kRpPageNotVisible:
    case FederatedRequestResult::kSilentMediationFailure:
    case FederatedRequestResult::kNotSignedInWithIdp:
    case FederatedRequestResult::kMissingTransientUserActivation:
    case FederatedRequestResult::kReplacedByActiveMode:
    case FederatedRequestResult::kRelyingPartyOriginIsOpaque:
    case FederatedRequestResult::kTypeNotMatching:
    case FederatedRequestResult::kUiDismissedNoEmbargo:
    case FederatedRequestResult::kCorsError:
    case FederatedRequestResult::kSuppressedBySegmentationPlatform:
    case FederatedRequestResult::kError: {
      return RequestTokenStatus::kError;
    }
  }
}

MetricsEndpointErrorCode FederatedRequestResultToMetricsEndpointErrorCode(
    blink::mojom::FederatedRequestResult result) {
  switch (result) {
    case FederatedRequestResult::kSuccess: {
      return MetricsEndpointErrorCode::kNone;
    }
    case FederatedRequestResult::kTooManyRequests:
    case FederatedRequestResult::kMissingTransientUserActivation:
    case FederatedRequestResult::kRelyingPartyOriginIsOpaque:
    case FederatedRequestResult::kCanceled: {
      return MetricsEndpointErrorCode::kRpFailure;
    }
    case FederatedRequestResult::kAccountsInvalidResponse:
    case FederatedRequestResult::kAccountsListEmpty:
    case FederatedRequestResult::kAccountsInvalidContentType: {
      return MetricsEndpointErrorCode::kAccountsEndpointInvalidResponse;
    }
    case FederatedRequestResult::kIdTokenInvalidResponse:
    case FederatedRequestResult::kIdTokenIdpErrorResponse:
    case FederatedRequestResult::kIdTokenCrossSiteIdpErrorResponse:
    case FederatedRequestResult::kIdTokenInvalidContentType:
    case FederatedRequestResult::kCorsError: {
      return MetricsEndpointErrorCode::kTokenEndpointInvalidResponse;
    }
    case FederatedRequestResult::kShouldEmbargo:
    case FederatedRequestResult::kUiDismissedNoEmbargo:
    case FederatedRequestResult::kDisabledInFlags:
    case FederatedRequestResult::kDisabledInSettings:
    case FederatedRequestResult::kRpPageNotVisible:
    case FederatedRequestResult::kReplacedByActiveMode:
    case FederatedRequestResult::kNotSignedInWithIdp: {
      return MetricsEndpointErrorCode::kUserFailure;
    }
    case FederatedRequestResult::kWellKnownHttpNotFound:
    case FederatedRequestResult::kWellKnownNoResponse:
    case FederatedRequestResult::kConfigHttpNotFound:
    case FederatedRequestResult::kConfigNoResponse:
    case FederatedRequestResult::kAccountsHttpNotFound:
    case FederatedRequestResult::kAccountsNoResponse:
    case FederatedRequestResult::kIdTokenHttpNotFound:
    case FederatedRequestResult::kIdTokenNoResponse: {
      return MetricsEndpointErrorCode::kIdpServerUnavailable;
    }
    case FederatedRequestResult::kConfigNotInWellKnown:
    case FederatedRequestResult::kWellKnownTooBig: {
      return MetricsEndpointErrorCode::kManifestError;
    }
    case FederatedRequestResult::kWellKnownListEmpty:
    case FederatedRequestResult::kWellKnownInvalidResponse:
    case FederatedRequestResult::kConfigInvalidResponse:
    case FederatedRequestResult::kWellKnownInvalidContentType:
    case FederatedRequestResult::kConfigInvalidContentType: {
      return MetricsEndpointErrorCode::kIdpServerInvalidResponse;
    }
    case FederatedRequestResult::kIdpNotPotentiallyTrustworthy:
    case FederatedRequestResult::kError:
    case FederatedRequestResult::kSilentMediationFailure:
    case FederatedRequestResult::kTypeNotMatching:
    case FederatedRequestResult::kSuppressedBySegmentationPlatform: {
      return MetricsEndpointErrorCode::kOther;
    }
  }
}

std::pair<FederatedRequestResult, RequestIdTokenStatus>
AccountParseStatusToRequestResultAndTokenStatus(ParseStatus parse_status) {
  switch (parse_status) {
    case ParseStatus::kHttpNotFoundError:
      return {FederatedRequestResult::kAccountsHttpNotFound,
              RequestIdTokenStatus::kAccountsHttpNotFound};
    case ParseStatus::kNoResponseError:
      return {FederatedRequestResult::kAccountsNoResponse,
              RequestIdTokenStatus::kAccountsNoResponse};
    case ParseStatus::kInvalidResponseError:
      return {FederatedRequestResult::kAccountsInvalidResponse,
              RequestIdTokenStatus::kAccountsInvalidResponse};
    case ParseStatus::kEmptyListError:
      return {FederatedRequestResult::kAccountsListEmpty,
              RequestIdTokenStatus::kAccountsListEmpty};
    case ParseStatus::kInvalidContentTypeError:
      return {FederatedRequestResult::kAccountsInvalidContentType,
              RequestIdTokenStatus::kAccountsInvalidContentType};
    case ParseStatus::kSuccess:
      NOTREACHED() << "Should not be invoked on success";
  }
}

LifecycleStateFailureReason
LifecycleStateImplLifecycleStateImplToFedCmLifecycleStateFailureReason(
    LifecycleStateImpl lifecycle_state) {
  switch (lifecycle_state) {
    case LifecycleStateImpl::kSpeculative:
      return LifecycleStateFailureReason::kSpeculative;
    case LifecycleStateImpl::kPendingCommit:
      return LifecycleStateFailureReason::kPendingCommit;
    case LifecycleStateImpl::kPrerendering:
      return LifecycleStateFailureReason::kPrerendering;
    case LifecycleStateImpl::kInBackForwardCache:
      return LifecycleStateFailureReason::kInBackForwardCache;
    case LifecycleStateImpl::kRunningUnloadHandlers:
      return LifecycleStateFailureReason::kRunningUnloadHandlers;
    case LifecycleStateImpl::kReadyToBeDeleted:
      return LifecycleStateFailureReason::kReadyToBeDeleted;
    default:
      return LifecycleStateFailureReason::kOther;
  }
}

std::pair<FederatedRequestResult, RequestIdTokenStatus>
PermissionStatusToRequestResultAndTokenStatus(
    FederatedIdentityApiPermissionContextDelegate::PermissionStatus
        permission_status) {
  switch (permission_status) {
    case FederatedApiPermissionStatus::BLOCKED_VARIATIONS:
      return {FederatedRequestResult::kDisabledInFlags,
              RequestIdTokenStatus::kDisabledInFlags};
    case FederatedApiPermissionStatus::BLOCKED_SETTINGS:
      return {FederatedRequestResult::kDisabledInSettings,
              RequestIdTokenStatus::kDisabledInSettings};
    case FederatedApiPermissionStatus::BLOCKED_EMBARGO:
      return {FederatedRequestResult::kDisabledInSettings,
              RequestIdTokenStatus::kDisabledEmbargo};
    case FederatedApiPermissionStatus::GRANTED:
      NOTREACHED() << "Should not be invoked with GRANTED";
  }
}

ErrorDialogResult DismissReasonToErrorDialogResult(
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

std::pair<FederatedRequestResult, RequestIdTokenStatus>
IdAssertionFetchStatusToRequestResultAndTokenStatus(FetchStatus status) {
  switch (status.parse_status) {
    case ParseStatus::kHttpNotFoundError:
      return {FederatedRequestResult::kIdTokenHttpNotFound,
              RequestIdTokenStatus::kIdTokenHttpNotFound};
    case ParseStatus::kNoResponseError: {
      if (status.cors_error) {
        return {FederatedRequestResult::kCorsError,
                RequestIdTokenStatus::kIdTokenNoResponse};
      }
      return {FederatedRequestResult::kIdTokenNoResponse,
              RequestIdTokenStatus::kIdTokenNoResponse};
    }
    case ParseStatus::kInvalidResponseError:
      return {FederatedRequestResult::kIdTokenInvalidResponse,
              RequestIdTokenStatus::kIdTokenInvalidResponse};
    case ParseStatus::kInvalidContentTypeError:
      return {FederatedRequestResult::kIdTokenInvalidContentType,
              RequestIdTokenStatus::kIdTokenInvalidContentType};
    case ParseStatus::kEmptyListError:
      NOTREACHED() << "EmptyListError is not an option for this fetch";
    case ParseStatus::kSuccess:
      NOTREACHED() << "Should not be invoked with success";
  }
}

EmailVerificationRequestResult WellKnownParseStatusToEvpRequestStatus(
    ParseStatus parse_status) {
  switch (parse_status) {
    case ParseStatus::kHttpNotFoundError:
      return EmailVerificationRequestResult::kWellKnownHttpNotFound;
    case ParseStatus::kNoResponseError:
      return EmailVerificationRequestResult::kWellKnownNoResponse;
    case ParseStatus::kInvalidResponseError:
      return EmailVerificationRequestResult::kWellKnownInvalidResponse;
    case ParseStatus::kEmptyListError:
      return EmailVerificationRequestResult::kWellKnownListEmpty;
    case ParseStatus::kInvalidContentTypeError:
      return EmailVerificationRequestResult::kWellKnownInvalidContentType;
    case ParseStatus::kSuccess:
      NOTREACHED();
  }
}

EmailVerificationRequestResult
EmailVerificationWellKnownParseStatusToEvpRequestStatus(
    ParseStatus parse_status) {
  switch (parse_status) {
    case ParseStatus::kHttpNotFoundError:
      return EmailVerificationRequestResult::
          kEmailVerificationWellKnownHttpNotFound;
    case ParseStatus::kNoResponseError:
      return EmailVerificationRequestResult::
          kEmailVerificationWellKnownNoResponse;
    case ParseStatus::kInvalidResponseError:
      return EmailVerificationRequestResult::
          kEmailVerificationWellKnownInvalidResponse;
    case ParseStatus::kEmptyListError:
      NOTREACHED() << "EmptyListError is not an option for this fetch";
    case ParseStatus::kInvalidContentTypeError:
      return EmailVerificationRequestResult::
          kEmailVerificationWellKnownInvalidContentType;
    case ParseStatus::kSuccess:
      NOTREACHED();
  }
}

EmailVerificationRequestResult AccountsListParseStatusToEvpRequestStatus(
    ParseStatus parse_status) {
  switch (parse_status) {
    case ParseStatus::kHttpNotFoundError:
      return EmailVerificationRequestResult::kAccountsHttpNotFound;
    case ParseStatus::kNoResponseError:
      return EmailVerificationRequestResult::kAccountsNoResponse;
    case ParseStatus::kInvalidResponseError:
      return EmailVerificationRequestResult::kAccountsInvalidResponse;
    case ParseStatus::kEmptyListError:
      return EmailVerificationRequestResult::kAccountsEmptyList;
    case ParseStatus::kInvalidContentTypeError:
      return EmailVerificationRequestResult::kAccountsInvalidContentType;
    case ParseStatus::kSuccess:
      NOTREACHED();
  }
}

EmailVerificationRequestResult TokenParseStatusToEvpRequestStatus(
    ParseStatus parse_status) {
  switch (parse_status) {
    case ParseStatus::kHttpNotFoundError:
      return EmailVerificationRequestResult::kTokenHttpNotFound;
    case ParseStatus::kNoResponseError:
      return EmailVerificationRequestResult::kTokenNoResponse;
    case ParseStatus::kInvalidResponseError:
      return EmailVerificationRequestResult::kTokenInvalidResponse;
    case ParseStatus::kInvalidContentTypeError:
      return EmailVerificationRequestResult::kTokenInvalidContentType;
    case ParseStatus::kEmptyListError:
    case ParseStatus::kSuccess:
      NOTREACHED();
  }
}

EmailVerificationRequestResult VerificationResultToEvpRequestStatus(
    EvtVerifier::Result result) {
  switch (result) {
    case EvtVerifier::Result::kVerified:
      return EmailVerificationRequestResult::kSuccess;
    case EvtVerifier::Result::kInvalidSdJwtKb:
      return EmailVerificationRequestResult::kTokenMalformedSdJwt;
    case EvtVerifier::Result::kSdJwtUnsupportedHeaderAlg:
      return EmailVerificationRequestResult::
          kTokenVerificationSdJwtUnsupportedHeaderAlg;
    case EvtVerifier::Result::kSdJwtInvalidTyp:
      return EmailVerificationRequestResult::kTokenVerificationSdJwtInvalidTyp;
    case EvtVerifier::Result::kSdJwtMissingIss:
      return EmailVerificationRequestResult::kTokenVerificationSdJwtMissingIss;
    case EvtVerifier::Result::kSdJwtMissingIat:
      return EmailVerificationRequestResult::kTokenVerificationSdJwtMissingIat;
    case EvtVerifier::Result::kSdJwtMissingCnf:
      return EmailVerificationRequestResult::kTokenVerificationSdJwtMissingCnf;
    case EvtVerifier::Result::kSdJwtMissingEmail:
      return EmailVerificationRequestResult::
          kTokenVerificationSdJwtMissingEmail;
    case EvtVerifier::Result::kSdJwtInvalidIssuedAt:
      return EmailVerificationRequestResult::
          kTokenVerificationSdJwtInvalidIssuedAt;
    case EvtVerifier::Result::kSdJwtInvalidIssuer:
      return EmailVerificationRequestResult::
          kTokenVerificationSdJwtInvalidIssuer;
    case EvtVerifier::Result::kSdJwtJwksMissingKeys:
      return EmailVerificationRequestResult::
          kTokenVerificationSdJwtJwksMissingKeys;
    case EvtVerifier::Result::kSdJwtSignatureFailed:
      return EmailVerificationRequestResult::
          kTokenVerificationSdJwtSignatureFailed;
    case EvtVerifier::Result::kSdJwtInvalidEmailVerified:
      return EmailVerificationRequestResult::
          kTokenVerificationSdJwtInvalidEmailVerified;
    case EvtVerifier::Result::kSdJwtInvalidEmail:
      return EmailVerificationRequestResult::
          kTokenVerificationSdJwtInvalidEmail;
    case EvtVerifier::Result::kSdJwtInvalidHolderKey:
      return EmailVerificationRequestResult::
          kTokenVerificationSdJwtInvalidHolderKey;
    case EvtVerifier::Result::kKbInvalidTyp:
      return EmailVerificationRequestResult::kTokenVerificationKbInvalidTyp;
    case EvtVerifier::Result::kKbMissingAud:
      return EmailVerificationRequestResult::kTokenVerificationKbMissingAud;
    case EvtVerifier::Result::kKbMissingNonce:
      return EmailVerificationRequestResult::kTokenVerificationKbMissingNonce;
    case EvtVerifier::Result::kKbMissingIat:
      return EmailVerificationRequestResult::kTokenVerificationKbMissingIat;
    case EvtVerifier::Result::kKbMissingSdHash:
      return EmailVerificationRequestResult::kTokenVerificationKbMissingSdHash;
    case EvtVerifier::Result::kKbInvalidIssuedAt:
      return EmailVerificationRequestResult::
          kTokenVerificationKbInvalidIssuedAt;
    case EvtVerifier::Result::kKbInvalidAudience:
      return EmailVerificationRequestResult::
          kTokenVerificationKbInvalidAudience;
    case EvtVerifier::Result::kKbInvalidNonce:
      return EmailVerificationRequestResult::kTokenVerificationKbInvalidNonce;
    case EvtVerifier::Result::kKbInvalidSdHash:
      return EmailVerificationRequestResult::kTokenVerificationKbInvalidSdHash;
    case EvtVerifier::Result::kKbMissingCnf:
      return EmailVerificationRequestResult::kTokenVerificationKbMissingCnf;
    case EvtVerifier::Result::kKbSignatureFailed:
      return EmailVerificationRequestResult::
          kTokenVerificationKbSignatureFailed;
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
    if (field == kDefaultFieldName) {
      list.push_back(IdentityRequestDialogDisclosureField::kName);
    } else if (field == kDefaultFieldEmail) {
      list.push_back(IdentityRequestDialogDisclosureField::kEmail);
    } else if (field == kDefaultFieldPicture) {
      list.push_back(IdentityRequestDialogDisclosureField::kPicture);
    } else if (field == kFieldPhoneNumber) {
      list.push_back(IdentityRequestDialogDisclosureField::kPhoneNumber);
    } else if (field == kFieldUsername) {
      list.push_back(IdentityRequestDialogDisclosureField::kUsername);
    }
  }
  return list;
}

void ComputeAccountFields(
    const std::vector<IdentityRequestDialogDisclosureField>& rp_fields,
    std::vector<scoped_refptr<IdentityRequestAccount>>& accounts) {
  for (const auto& account : accounts) {
    account->fields.clear();
    if (account->idp_claimed_login_state.value_or(
            account->browser_trusted_login_state) ==
        IdentityRequestAccount::LoginState::kSignIn) {
      // We only show fields for signups.
      continue;
    }
    for (auto field : rp_fields) {
      switch (field) {
        case IdentityRequestDialogDisclosureField::kName:
          if (!account->name.empty()) {
            account->fields.push_back(field);
          }
          break;
        case IdentityRequestDialogDisclosureField::kEmail:
          if (!account->email.empty()) {
            account->fields.push_back(field);
          }
          break;
        case IdentityRequestDialogDisclosureField::kPicture:
          if (account->picture.is_valid()) {
            account->fields.push_back(field);
          }
          break;
        case IdentityRequestDialogDisclosureField::kPhoneNumber:
          if (!account->phone.empty()) {
            account->fields.push_back(field);
          }
          break;
        case IdentityRequestDialogDisclosureField::kUsername:
          if (!account->username.empty()) {
            account->fields.push_back(field);
          }
          break;
      };
    }
  }
}

FederatedLoginResult FederatedRequestResultToFederatedLoginResult(
    FederatedRequestResult result) {
  FederatedLoginResult federated_login_result;
  switch (result) {
    case blink::mojom::FederatedRequestResult::kSuccess:
      federated_login_result = FederatedLoginResult::kSuccess;
      break;
    case blink::mojom::FederatedRequestResult::kIdpNotPotentiallyTrustworthy:
    case blink::mojom::FederatedRequestResult::kWellKnownHttpNotFound:
    case blink::mojom::FederatedRequestResult::kWellKnownNoResponse:
    case blink::mojom::FederatedRequestResult::kWellKnownInvalidResponse:
    case blink::mojom::FederatedRequestResult::kWellKnownListEmpty:
    case blink::mojom::FederatedRequestResult::kWellKnownInvalidContentType:
    case blink::mojom::FederatedRequestResult::kConfigNotInWellKnown:
    case blink::mojom::FederatedRequestResult::kWellKnownTooBig:
    case blink::mojom::FederatedRequestResult::kConfigHttpNotFound:
    case blink::mojom::FederatedRequestResult::kConfigNoResponse:
    case blink::mojom::FederatedRequestResult::kConfigInvalidResponse:
    case blink::mojom::FederatedRequestResult::kConfigInvalidContentType:
    case blink::mojom::FederatedRequestResult::kAccountsHttpNotFound:
    case blink::mojom::FederatedRequestResult::kAccountsNoResponse:
    case blink::mojom::FederatedRequestResult::kAccountsInvalidResponse:
    case blink::mojom::FederatedRequestResult::kAccountsListEmpty:
    case blink::mojom::FederatedRequestResult::kAccountsInvalidContentType:
    case blink::mojom::FederatedRequestResult::kIdTokenHttpNotFound:
    case blink::mojom::FederatedRequestResult::kIdTokenNoResponse:
    case blink::mojom::FederatedRequestResult::kIdTokenInvalidResponse:
    case blink::mojom::FederatedRequestResult::kIdTokenInvalidContentType:
    case blink::mojom::FederatedRequestResult::kRelyingPartyOriginIsOpaque:
    case blink::mojom::FederatedRequestResult::kTypeNotMatching:
    case blink::mojom::FederatedRequestResult::kError:
    case blink::mojom::FederatedRequestResult::kCorsError:
      federated_login_result = FederatedLoginResult::kIdpNetworkError;
      break;
    case blink::mojom::FederatedRequestResult::kIdTokenIdpErrorResponse:
    case blink::mojom::FederatedRequestResult::
        kIdTokenCrossSiteIdpErrorResponse:
      federated_login_result = FederatedLoginResult::kIdpReturnedError;
      break;
    case blink::mojom::FederatedRequestResult::kCanceled:
      federated_login_result = FederatedLoginResult::kTokenRequestAborted;
      break;
    case blink::mojom::FederatedRequestResult::kRpPageNotVisible:
      federated_login_result = FederatedLoginResult::kFrameNotActive;
      break;
    case blink::mojom::FederatedRequestResult::kSilentMediationFailure:
      federated_login_result = FederatedLoginResult::kExpectedAccountNotPresent;
      break;
    case blink::mojom::FederatedRequestResult::kNotSignedInWithIdp:
      federated_login_result = FederatedLoginResult::kAccountNotLoggedIn;
      break;
    // These should not happen during actor login flow, but this conversion
    // method is invoked regardless, so just return some default error.
    case blink::mojom::FederatedRequestResult::kShouldEmbargo:
    case blink::mojom::FederatedRequestResult::kUiDismissedNoEmbargo:
    case blink::mojom::FederatedRequestResult::kDisabledInSettings:
    case blink::mojom::FederatedRequestResult::kDisabledInFlags:
    case blink::mojom::FederatedRequestResult::kTooManyRequests:
    case blink::mojom::FederatedRequestResult::kMissingTransientUserActivation:
    case blink::mojom::FederatedRequestResult::kReplacedByActiveMode:
    case blink::mojom::FederatedRequestResult::
        kSuppressedBySegmentationPlatform:
      federated_login_result = FederatedLoginResult::kIdpNetworkError;
      break;
  }
  return federated_login_result;
}

}  // namespace content::webid
