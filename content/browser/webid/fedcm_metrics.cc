// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/fedcm_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/types/pass_key.h"
#include "content/browser/webid/flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "url/gurl.h"

namespace content {

FedCmMetrics::FedCmMetrics(const GURL& provider,
                           ukm::SourceId page_source_id,
                           int session_id,
                           bool is_disabled)
    : page_source_id_(page_source_id),
      provider_source_id_(ukm::UkmRecorder::GetSourceIdForWebIdentityFromScope(
          base::PassKey<FedCmMetrics>(),
          provider)),
      session_id_(session_id),
      is_disabled_(is_disabled) {}

void FedCmMetrics::RecordShowAccountsDialogTime(base::TimeDelta duration) {
  if (is_disabled_)
    return;
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetTiming_ShowAccountsDialog(
        ukm::GetExponentialBucketMinForUserTiming(duration.InMilliseconds()));
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);
  ukm::builders::Blink_FedCmIdp fedcm_idp_builder(provider_source_id_);
  RecordUkm(fedcm_idp_builder);

  base::UmaHistogramMediumTimes("Blink.FedCm.Timing.ShowAccountsDialog",
                                duration);
}

void FedCmMetrics::RecordContinueOnDialogTime(base::TimeDelta duration) {
  if (is_disabled_)
    return;
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetTiming_ContinueOnDialog(
        ukm::GetExponentialBucketMinForUserTiming(duration.InMilliseconds()));
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);

  ukm::builders::Blink_FedCmIdp fedcm_idp_builder(provider_source_id_);
  RecordUkm(fedcm_idp_builder);

  base::UmaHistogramMediumTimes("Blink.FedCm.Timing.ContinueOnDialog",
                                duration);
}

void FedCmMetrics::RecordCancelOnDialogTime(base::TimeDelta duration) {
  if (is_disabled_)
    return;
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetTiming_CancelOnDialog(
        ukm::GetExponentialBucketMinForUserTiming(duration.InMilliseconds()));
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);

  ukm::builders::Blink_FedCmIdp fedcm_idp_builder(provider_source_id_);
  RecordUkm(fedcm_idp_builder);

  base::UmaHistogramMediumTimes("Blink.FedCm.Timing.CancelOnDialog", duration);
}

void FedCmMetrics::RecordAccountsDialogShownDuration(base::TimeDelta duration) {
  if (is_disabled_) {
    return;
  }
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetTiming_AccountsDialogShownDuration(
        ukm::GetExponentialBucketMinForUserTiming(duration.InMilliseconds()));
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);
  ukm::builders::Blink_FedCmIdp fedcm_idp_builder(provider_source_id_);
  RecordUkm(fedcm_idp_builder);

  // Samples are at most 10 minutes. This metric is used to determine a
  // reasonable minimum duration for the accounts dialog to be shown to
  // prevent abuse through flashing UI so a higher maximum is not needed.
  base::UmaHistogramCustomTimes(
      "Blink.FedCm.Timing.AccountsDialogShownDuration2", duration,
      base::Milliseconds(1), base::Minutes(10), 50);
}

void FedCmMetrics::RecordMismatchDialogShownDuration(base::TimeDelta duration) {
  if (is_disabled_) {
    return;
  }
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetTiming_MismatchDialogShownDuration(
        ukm::GetExponentialBucketMinForUserTiming(duration.InMilliseconds()));
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);
  ukm::builders::Blink_FedCmIdp fedcm_idp_builder(provider_source_id_);
  RecordUkm(fedcm_idp_builder);

  // Samples are at most 10 minutes. This metric is used to determine a
  // reasonable minimum duration for the mismatch dialog to be shown to
  // prevent abuse through flashing UI so a higher maximum is not needed.
  base::UmaHistogramCustomTimes(
      "Blink.FedCm.Timing.MismatchDialogShownDuration", duration,
      base::Milliseconds(1), base::Minutes(10), 50);
}

void FedCmMetrics::RecordCancelReason(
    IdentityRequestDialogController::DismissReason dismiss_reason) {
  if (is_disabled_)
    return;
  base::UmaHistogramEnumeration("Blink.FedCm.CancelReason", dismiss_reason);
}

void FedCmMetrics::RecordTokenResponseAndTurnaroundTime(
    base::TimeDelta token_response_time,
    base::TimeDelta turnaround_time) {
  if (is_disabled_)
    return;
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder
        .SetTiming_IdTokenResponse(ukm::GetExponentialBucketMinForUserTiming(
            token_response_time.InMilliseconds()))
        .SetTiming_TurnaroundTime(ukm::GetExponentialBucketMinForUserTiming(
            turnaround_time.InMilliseconds()));
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);

  ukm::builders::Blink_FedCmIdp fedcm_idp_builder(provider_source_id_);
  RecordUkm(fedcm_idp_builder);

  base::UmaHistogramMediumTimes("Blink.FedCm.Timing.IdTokenResponse",
                                token_response_time);
  base::UmaHistogramMediumTimes("Blink.FedCm.Timing.TurnaroundTime",
                                turnaround_time);
}

void FedCmMetrics::RecordRequestTokenStatus(FedCmRequestIdTokenStatus status,
                                            MediationRequirement requirement) {
  if (is_disabled_)
    return;
  // If the request has failed but we have not yet rejected the promise,
  // e.g. when the user has declined the permission or the API is disabled
  // etc., we have already recorded a RequestTokenStatus. i.e.
  // `request_token_status_recorded_` would be true. In this case, we
  // shouldn't record another RequestTokenStatus.
  if (request_token_status_recorded_) {
    return;
  }
  request_token_status_recorded_ = true;

  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetStatus_RequestIdToken(static_cast<int>(status));
    ukm_builder.SetStatus_MediationRequirement(static_cast<int>(requirement));
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);

  ukm::builders::Blink_FedCmIdp fedcm_idp_builder(provider_source_id_);
  RecordUkm(fedcm_idp_builder);

  base::UmaHistogramEnumeration("Blink.FedCm.Status.RequestIdToken", status);
  base::UmaHistogramEnumeration("Blink.FedCm.Status.MediationRequirement",
                                requirement);
}

void FedCmMetrics::RecordSignInStateMatchStatus(
    FedCmSignInStateMatchStatus status) {
  if (is_disabled_)
    return;
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetStatus_SignInStateMatch(static_cast<int>(status));
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };

  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);

  ukm::builders::Blink_FedCmIdp fedcm_idp_builder(provider_source_id_);
  RecordUkm(fedcm_idp_builder);

  base::UmaHistogramEnumeration("Blink.FedCm.Status.SignInStateMatch", status);
}

void FedCmMetrics::RecordIdpSigninMatchStatus(
    absl::optional<bool> idp_signin_status,
    IdpNetworkRequestManager::ParseStatus accounts_endpoint_status) {
  if (is_disabled_)
    return;

  FedCmIdpSigninMatchStatus match_status = FedCmIdpSigninMatchStatus::kMaxValue;
  if (!idp_signin_status.has_value()) {
    match_status =
        (accounts_endpoint_status ==
         IdpNetworkRequestManager::ParseStatus::kSuccess)
            ? FedCmIdpSigninMatchStatus::kUnknownStatusWithAccounts
            : FedCmIdpSigninMatchStatus::kUnknownStatusWithoutAccounts;
  } else if (idp_signin_status.value()) {
    switch (accounts_endpoint_status) {
      case IdpNetworkRequestManager::ParseStatus::kHttpNotFoundError:
        match_status = FedCmIdpSigninMatchStatus::kMismatchWithNetworkError;
        break;
      case IdpNetworkRequestManager::ParseStatus::kNoResponseError:
        match_status = FedCmIdpSigninMatchStatus::kMismatchWithNoContent;
        break;
      case IdpNetworkRequestManager::ParseStatus::kInvalidResponseError:
        match_status = FedCmIdpSigninMatchStatus::kMismatchWithInvalidResponse;
        break;
      case IdpNetworkRequestManager::ParseStatus::kEmptyListError:
        match_status = FedCmIdpSigninMatchStatus::kMismatchWithNoContent;
        break;
      case IdpNetworkRequestManager::ParseStatus::kInvalidContentTypeError:
        match_status = FedCmIdpSigninMatchStatus::kMismatchWithInvalidResponse;
        break;
      case IdpNetworkRequestManager::ParseStatus::kSuccess:
        match_status = FedCmIdpSigninMatchStatus::kMatchWithAccounts;
        break;
    }
  } else {
    match_status =
        (accounts_endpoint_status ==
         IdpNetworkRequestManager::ParseStatus::kSuccess)
            ? FedCmIdpSigninMatchStatus::kMismatchWithUnexpectedAccounts
            : FedCmIdpSigninMatchStatus::kMatchWithoutAccounts;
  }

  base::UmaHistogramEnumeration("Blink.FedCm.Status.IdpSigninMatch",
                                match_status);
}

void FedCmMetrics::RecordIsSignInUser(bool is_sign_in) {
  if (is_disabled_)
    return;
  base::UmaHistogramBoolean("Blink.FedCm.IsSignInUser", is_sign_in);
}

void FedCmMetrics::RecordWebContentsVisibilityUponReadyToShowDialog(
    bool is_visible) {
  if (is_disabled_)
    return;
  base::UmaHistogramBoolean("Blink.FedCm.WebContentsVisible", is_visible);
}

void FedCmMetrics::RecordAutoReauthnMetrics(
    absl::optional<bool> has_single_returning_account,
    const IdentityRequestAccount* auto_signin_account,
    bool auto_reauthn_success,
    bool is_auto_reauthn_setting_blocked,
    bool is_auto_reauthn_embargoed,
    absl::optional<base::TimeDelta> time_from_embargo,
    bool requires_user_mediation) {
  NumAccounts num_returning_accounts = NumAccounts::kZero;
  if (has_single_returning_account.has_value()) {
    if (*has_single_returning_account) {
      num_returning_accounts = NumAccounts::kOne;
    } else if (auto_signin_account) {
      num_returning_accounts = NumAccounts::kMultiple;
    }

    base::UmaHistogramEnumeration("Blink.FedCm.AutoReauthn.ReturningAccounts",
                                  num_returning_accounts);
  }
  base::UmaHistogramBoolean("Blink.FedCm.AutoReauthn.Succeeded",
                            auto_reauthn_success);
  base::UmaHistogramBoolean("Blink.FedCm.AutoReauthn.BlockedByContentSettings",
                            is_auto_reauthn_setting_blocked);
  base::UmaHistogramBoolean("Blink.FedCm.AutoReauthn.BlockedByEmbargo",
                            is_auto_reauthn_embargoed);
  base::UmaHistogramBoolean(
      "Blink.FedCm.AutoReauthn.BlockedByPreventSilentAccess",
      requires_user_mediation);
  ukm::builders::Blink_FedCm ukm_builder(page_source_id_);
  if (time_from_embargo) {
    // Use a custom histogram with the default number of buckets so that we set
    // the maximum to the permission embargo duration: 10 minutes. See
    // `kFederatedIdentityAutoReauthnEmbargoDuration`.
    base::UmaHistogramCustomTimes(
        "Blink.FedCm.AutoReauthn.TimeFromEmbargoWhenBlocked",
        *time_from_embargo, base::Milliseconds(10), base::Minutes(10),
        /*buckets=*/50);
    ukm_builder.SetAutoReauthn_TimeFromEmbargoWhenBlocked(
        ukm::GetExponentialBucketMinForUserTiming(
            time_from_embargo->InMilliseconds()));
  }

  if (has_single_returning_account.has_value()) {
    ukm_builder.SetAutoReauthn_ReturningAccounts(
        static_cast<int>(num_returning_accounts));
  }
  ukm_builder.SetAutoReauthn_Succeeded(auto_reauthn_success);
  ukm_builder.SetAutoReauthn_BlockedByContentSettings(
      is_auto_reauthn_setting_blocked);
  ukm_builder.SetAutoReauthn_BlockedByEmbargo(is_auto_reauthn_embargoed);
  ukm_builder.SetAutoReauthn_BlockedByPreventSilentAccess(
      requires_user_mediation);
  ukm_builder.SetFedCmSessionID(session_id_);
  ukm_builder.Record(ukm::UkmRecorder::Get());
}

void FedCmMetrics::RecordAccountsDialogShown() {
  if (is_disabled_) {
    return;
  }
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetAccountsDialogShown(true);
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);

  ukm::builders::Blink_FedCmIdp fedcm_idp_builder(provider_source_id_);
  RecordUkm(fedcm_idp_builder);

  base::UmaHistogramBoolean("Blink.FedCm.AccountsDialogShown", true);
}

void FedCmMetrics::RecordMismatchDialogShown() {
  if (is_disabled_) {
    return;
  }
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetMismatchDialogShown(true);
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);

  ukm::builders::Blink_FedCmIdp fedcm_idp_builder(provider_source_id_);
  RecordUkm(fedcm_idp_builder);

  base::UmaHistogramBoolean("Blink.FedCm.MismatchDialogShown", true);
}

void FedCmMetrics::RecordAccountsRequestSent() {
  if (is_disabled_) {
    return;
  }
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetAccountsRequestSent(true);
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);

  ukm::builders::Blink_FedCmIdp fedcm_idp_builder(provider_source_id_);
  RecordUkm(fedcm_idp_builder);

  base::UmaHistogramBoolean("Blink.FedCm.AccountsRequestSent", true);
}

void FedCmMetrics::RecordNumRequestsPerDocument(const int num_requests) {
  if (is_disabled_) {
    return;
  }
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetNumRequestsPerDocument(num_requests);
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);

  base::UmaHistogramCounts100("Blink.FedCm.NumRequestsPerDocument",
                              num_requests);
}

void FedCmMetrics::RecordDisconnectStatus(FedCmDisconnectStatus status) {
  if (is_disabled_) {
    return;
  }
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetStatus_Disconnect(static_cast<int>(status));
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);

  ukm::builders::Blink_FedCmIdp fedcm_idp_builder(provider_source_id_);
  RecordUkm(fedcm_idp_builder);

  base::UmaHistogramEnumeration("Blink.FedCm.Status.Disconnect", status);
}

void FedCmMetrics::RecordErrorDialogResult(FedCmErrorDialogResult result) {
  if (is_disabled_) {
    return;
  }
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetError_ErrorDialogResult(static_cast<int>(result));
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);

  ukm::builders::Blink_FedCmIdp fedcm_idp_builder(provider_source_id_);
  RecordUkm(fedcm_idp_builder);

  base::UmaHistogramEnumeration("Blink.FedCm.Error.ErrorDialogResult", result);
}

void FedCmMetrics::RecordErrorDialogType(
    IdpNetworkRequestManager::FedCmErrorDialogType type) {
  if (is_disabled_) {
    return;
  }
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetError_ErrorDialogType(static_cast<int>(type));
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);

  ukm::builders::Blink_FedCmIdp fedcm_idp_builder(provider_source_id_);
  RecordUkm(fedcm_idp_builder);

  base::UmaHistogramEnumeration("Blink.FedCm.Error.ErrorDialogType", type);
}

void FedCmMetrics::RecordTokenResponseTypeMetrics(
    IdpNetworkRequestManager::FedCmTokenResponseType type) {
  if (is_disabled_) {
    return;
  }
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetError_TokenResponseType(static_cast<int>(type));
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);

  ukm::builders::Blink_FedCmIdp fedcm_idp_builder(provider_source_id_);
  RecordUkm(fedcm_idp_builder);

  base::UmaHistogramEnumeration("Blink.FedCm.Error.TokenResponseType", type);
}

void FedCmMetrics::RecordErrorUrlTypeMetrics(
    IdpNetworkRequestManager::FedCmErrorUrlType type) {
  if (is_disabled_) {
    return;
  }
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetError_ErrorUrlType(static_cast<int>(type));
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  // We do not record the RP-keyed equivalent because the error URL is passed by
  // the IDP.
  ukm::builders::Blink_FedCmIdp fedcm_idp_builder(provider_source_id_);
  RecordUkm(fedcm_idp_builder);

  base::UmaHistogramEnumeration("Blink.FedCm.Error.ErrorUrlType", type);
}

void RecordPreventSilentAccess(RenderFrameHost& rfh,
                               PreventSilentAccessFrameType frame_type) {
  base::UmaHistogramEnumeration("Blink.FedCm.PreventSilentAccessFrameType",
                                frame_type);

  // Ensure the lifecycle state as GetPageUkmSourceId doesn't support the
  // prerendering page. As FederatedAithRequest runs behind the
  // BrowserInterfaceBinders, the service doesn't receive any request while
  // prerendering, and the CHECK should always meet the condition.
  CHECK(
      !rfh.IsInLifecycleState(RenderFrameHost::LifecycleState::kPrerendering));
  ukm::builders::Blink_FedCm ukm_builder(rfh.GetPageUkmSourceId());
  ukm_builder.SetPreventSilentAccessFrameType(static_cast<int>(frame_type));
  ukm_builder.Record(ukm::UkmRecorder::Get());
}

void RecordApprovedClientsExistence(bool has_approved_clients) {
  if (IsFedCmMultipleIdentityProvidersEnabled())
    return;
  base::UmaHistogramBoolean("Blink.FedCm.ApprovedClientsExistence",
                            has_approved_clients);
}

void RecordApprovedClientsSize(int size) {
  if (IsFedCmMultipleIdentityProvidersEnabled())
    return;
  base::UmaHistogramCounts10000("Blink.FedCm.ApprovedClientsSize", size);
}

void RecordIdpSignOutNetError(int response_code) {
  int net_error = net::OK;
  if (response_code < 0) {
    // In this case, we got a net error, so change |net_error| to the value.
    net_error = response_code;
  }
  base::UmaHistogramSparse("Blink.FedCm.SignInStatusSetToSignout.NetError",
                           -net_error);
}

void RecordAccountsResponseInvalidReason(
    IdpNetworkRequestManager::AccountsResponseInvalidReason reason) {
  base::UmaHistogramEnumeration(
      "Blink.FedCm.Status.AccountsResponseInvalidReason", reason);
}

void RecordSetLoginStatusIgnoredReason(
    FedCmSetLoginStatusIgnoredReason reason) {
  base::UmaHistogramEnumeration("Blink.FedCm.SetLoginStatusIgnored", reason);
}

}  // namespace content
