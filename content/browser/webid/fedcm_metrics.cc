// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/fedcm_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/types/pass_key.h"
#include "content/browser/webid/flags.h"
#include "content/browser/webid/webid_utils.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

FedCmRequesterFrameType ComputeRequesterFrameType(const RenderFrameHost& rfh,
                                                  url::Origin requester,
                                                  url::Origin embedder) {
  // Since FedCM methods are not supported in FencedFrames, we can know whether
  // this is a main frame by calling GetParent().
  if (!rfh.GetParent()) {
    return FedCmRequesterFrameType::kMainFrame;
  }
  std::string requester_str =
      webid::FormatUrlWithDomain(requester.GetURL(), /*for_display=*/false);
  std::string embedder_str = webid::FormatUrlWithDomain(embedder.GetURL(),
                                                        /*for_display=*/false);
  if (requester_str == embedder_str) {
    return FedCmRequesterFrameType::kSameSiteIframe;
  }
  return FedCmRequesterFrameType::kCrossSiteIframe;
}

}  // namespace

FedCmMetrics::FedCmMetrics(const GURL& provider,
                           ukm::SourceId page_source_id,
                           int session_id)
    : page_source_id_(page_source_id), session_id_(session_id) {
  // TODO(crbug.com/326397737): Remove the |provider| parameter from the
  // constructor.
  ukm::SourceId source_id =
      ukm::UkmRecorder::GetSourceIdForWebIdentityFromScope(
          base::PassKey<FedCmMetrics>(), provider);
  provider_source_ids_[provider] = source_id;
  provider_source_id_ = source_id;
}

FedCmMetrics::~FedCmMetrics() = default;

void FedCmMetrics::RecordShowAccountsDialogTime(
    const std::vector<IdentityProviderData>& providers,
    base::TimeDelta duration) {
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetTiming_ShowAccountsDialog(
        ukm::GetExponentialBucketMinForUserTiming(duration.InMilliseconds()));
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);
  for (const auto& provider : providers) {
    // A provider may have no accounts, for instance if present due to IDP
    // mismatch.
    if (!provider.accounts.empty()) {
      ukm::builders::Blink_FedCmIdp fedcm_idp_builder(
          GetOrCreateProviderSourceId(provider.idp_metadata.config_url));
      RecordUkm(fedcm_idp_builder);
    }
  }

  base::UmaHistogramMediumTimes("Blink.FedCm.Timing.ShowAccountsDialog",
                                duration);
}

void FedCmMetrics::RecordShowAccountsDialogTimeBreakdown(
    base::TimeDelta well_known_and_config_fetch_duration,
    base::TimeDelta accounts_fetch_duration,
    base::TimeDelta client_metadata_fetch_duration) {
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetTiming_ShowAccountsDialogBreakdown_WellKnownAndConfigFetch(
        ukm::GetExponentialBucketMinForUserTiming(
            well_known_and_config_fetch_duration.InMilliseconds()));
    ukm_builder.SetTiming_ShowAccountsDialogBreakdown_AccountsFetch(
        ukm::GetExponentialBucketMinForUserTiming(
            accounts_fetch_duration.InMilliseconds()));
    ukm_builder.SetTiming_ShowAccountsDialogBreakdown_ClientMetadataFetch(
        ukm::GetExponentialBucketMinForUserTiming(
            client_metadata_fetch_duration.InMilliseconds()));
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);

  base::UmaHistogramMediumTimes(
      "Blink.FedCm.Timing.ShowAccountsDialogBreakdown.WellKnownAndConfigFetch",
      well_known_and_config_fetch_duration);
  base::UmaHistogramMediumTimes(
      "Blink.FedCm.Timing.ShowAccountsDialogBreakdown.AccountsFetch",
      accounts_fetch_duration);
  base::UmaHistogramMediumTimes(
      "Blink.FedCm.Timing.ShowAccountsDialogBreakdown.ClientMetadataFetch",
      client_metadata_fetch_duration);
}

void FedCmMetrics::RecordWellKnownAndConfigFetchTime(base::TimeDelta duration) {
  base::UmaHistogramMediumTimes("Blink.FedCm.Timing.WellKnownAndConfigFetch",
                                duration);
}

// static
void FedCmMetrics::RecordNumRequestsPerDocument(ukm::SourceId page_source_id,
                                                const int num_requests) {
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetNumRequestsPerDocument(num_requests);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id);
  RecordUkm(fedcm_builder);

  base::UmaHistogramCounts100("Blink.FedCm.NumRequestsPerDocument",
                              num_requests);
}

void FedCmMetrics::RecordContinueOnDialogTime(const GURL& provider,
                                              base::TimeDelta duration) {
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetTiming_ContinueOnDialog(
        ukm::GetExponentialBucketMinForUserTiming(duration.InMilliseconds()));
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);

  ukm::builders::Blink_FedCmIdp fedcm_idp_builder(
      GetOrCreateProviderSourceId(provider));
  RecordUkm(fedcm_idp_builder);

  base::UmaHistogramMediumTimes("Blink.FedCm.Timing.ContinueOnDialog",
                                duration);
}

void FedCmMetrics::RecordCancelOnDialogTime(
    const std::vector<IdentityProviderData>& providers,
    base::TimeDelta duration) {
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetTiming_CancelOnDialog(
        ukm::GetExponentialBucketMinForUserTiming(duration.InMilliseconds()));
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);

  for (const auto& provider : providers) {
    ukm::builders::Blink_FedCmIdp fedcm_idp_builder(
        GetOrCreateProviderSourceId(provider.idp_metadata.config_url));
    RecordUkm(fedcm_idp_builder);
  }

  base::UmaHistogramMediumTimes("Blink.FedCm.Timing.CancelOnDialog", duration);
}

void FedCmMetrics::RecordAccountsDialogShownDuration(
    const std::vector<IdentityProviderData>& providers,
    base::TimeDelta duration) {
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetTiming_AccountsDialogShownDuration(
        ukm::GetExponentialBucketMinForUserTiming(duration.InMilliseconds()));
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);

  for (const auto& provider : providers) {
    ukm::builders::Blink_FedCmIdp fedcm_idp_builder(
        GetOrCreateProviderSourceId(provider.idp_metadata.config_url));
    // A provider may have no accounts and be present due to IDP mismatch.
    if (!provider.accounts.empty()) {
      RecordUkm(fedcm_idp_builder);
    } else {
      DCHECK(provider.has_login_status_mismatch);
      fedcm_idp_builder.SetTiming_MismatchDialogShownDuration(
          ukm::GetExponentialBucketMinForUserTiming(duration.InMilliseconds()));
      fedcm_idp_builder.SetFedCmSessionID(session_id_);
      fedcm_idp_builder.Record(ukm::UkmRecorder::Get());
    }
  }

  // Samples are at most 10 minutes. This metric is used to determine a
  // reasonable minimum duration for the accounts dialog to be shown to
  // prevent abuse through flashing UI so a higher maximum is not needed.
  base::UmaHistogramCustomTimes(
      "Blink.FedCm.Timing.AccountsDialogShownDuration2", duration,
      base::Milliseconds(1), base::Minutes(10), 50);
}

void FedCmMetrics::RecordMismatchDialogShownDuration(
    const std::vector<IdentityProviderData>& providers,
    base::TimeDelta duration) {
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetTiming_MismatchDialogShownDuration(
        ukm::GetExponentialBucketMinForUserTiming(duration.InMilliseconds()));
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);

  for (const auto& provider : providers) {
    // We should only reach this if all `providers` are a mismatch.
    ukm::builders::Blink_FedCmIdp fedcm_idp_builder(
        GetOrCreateProviderSourceId(provider.idp_metadata.config_url));
    RecordUkm(fedcm_idp_builder);
  }

  // Samples are at most 10 minutes. This metric is used to determine a
  // reasonable minimum duration for the mismatch dialog to be shown to
  // prevent abuse through flashing UI so a higher maximum is not needed.
  base::UmaHistogramCustomTimes(
      "Blink.FedCm.Timing.MismatchDialogShownDuration", duration,
      base::Milliseconds(1), base::Minutes(10), 50);
}

void FedCmMetrics::RecordCancelReason(
    IdentityRequestDialogController::DismissReason dismiss_reason) {
  base::UmaHistogramEnumeration("Blink.FedCm.CancelReason", dismiss_reason);
}

void FedCmMetrics::RecordTokenResponseAndTurnaroundTime(
    const GURL& provider,
    base::TimeDelta token_response_time,
    base::TimeDelta turnaround_time) {
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

  ukm::builders::Blink_FedCmIdp fedcm_idp_builder(
      GetOrCreateProviderSourceId(provider));
  RecordUkm(fedcm_idp_builder);

  base::UmaHistogramMediumTimes("Blink.FedCm.Timing.IdTokenResponse",
                                token_response_time);
  base::UmaHistogramMediumTimes("Blink.FedCm.Timing.TurnaroundTime",
                                turnaround_time);
}

void FedCmMetrics::RecordRequestTokenStatus(
    FedCmRequestIdTokenStatus status,
    MediationRequirement requirement,
    const std::vector<GURL>& requested_providers,
    int num_idps_mismatch,
    const std::optional<GURL>& selected_idp_config_url,
    const RpMode& rp_mode) {
  // If the request has failed but we have not yet rejected the promise,
  // e.g. when the user has declined the permission or the API is disabled
  // etc., we have already recorded a RequestTokenStatus. i.e.
  // `request_token_status_recorded_` would be true. In this case, we
  // shouldn't record another RequestTokenStatus.
  if (request_token_status_recorded_) {
    return;
  }
  request_token_status_recorded_ = true;

  // Use exponential bucketing to log these numbers.
  num_idps_mismatch =
      ukm::GetExponentialBucketMinForCounts1000(num_idps_mismatch);
  int num_idps_requested =
      ukm::GetExponentialBucketMinForCounts1000(requested_providers.size());

  auto RecordUkm = [&](auto& ukm_builder,
                       FedCmRequestIdTokenStatus ukm_status) {
    ukm_builder.SetStatus_RequestIdToken(static_cast<int>(ukm_status));
    ukm_builder.SetStatus_MediationRequirement(static_cast<int>(requirement));
    ukm_builder.SetNumIdpsRequested(num_idps_requested);
    ukm_builder.SetNumIdpsMismatch(num_idps_mismatch);
    ukm_builder.SetRpMode(static_cast<int>(rp_mode));
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder, status);

  for (const auto& provider : requested_providers) {
    ukm::builders::Blink_FedCmIdp fedcm_idp_builder(
        GetOrCreateProviderSourceId(provider));
    if (status == FedCmRequestIdTokenStatus::kSuccess) {
      CHECK(selected_idp_config_url);
      if (provider == *selected_idp_config_url) {
        RecordUkm(fedcm_idp_builder, status);
      } else {
        RecordUkm(fedcm_idp_builder,
                  FedCmRequestIdTokenStatus::kOtherIdpChosen);
      }
    } else {
      RecordUkm(fedcm_idp_builder, status);
    }
  }

  base::UmaHistogramEnumeration("Blink.FedCm.Status.RequestIdToken", status);
  base::UmaHistogramEnumeration("Blink.FedCm.Status.MediationRequirement",
                                requirement);
}

void FedCmMetrics::RecordSignInStateMatchStatus(
    const GURL& provider,
    FedCmSignInStateMatchStatus status) {
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetStatus_SignInStateMatch(static_cast<int>(status));
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };

  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);

  ukm::builders::Blink_FedCmIdp fedcm_idp_builder(
      GetOrCreateProviderSourceId(provider));
  RecordUkm(fedcm_idp_builder);

  base::UmaHistogramEnumeration("Blink.FedCm.Status.SignInStateMatch", status);
}

void FedCmMetrics::RecordIdpSigninMatchStatus(
    std::optional<bool> idp_signin_status,
    IdpNetworkRequestManager::ParseStatus accounts_endpoint_status) {
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
  base::UmaHistogramBoolean("Blink.FedCm.IsSignInUser", is_sign_in);
}

void FedCmMetrics::RecordWebContentsStatusUponReadyToShowDialog(
    bool is_visible,
    bool is_active) {
  base::UmaHistogramBoolean("Blink.FedCm.WebContentsVisible", is_visible);
  base::UmaHistogramBoolean("Blink.FedCm.WebContentsActive", is_active);
}

void FedCmMetrics::RecordAutoReauthnMetrics(
    std::optional<bool> has_single_returning_account,
    const IdentityRequestAccount* auto_signin_account,
    bool auto_reauthn_success,
    bool is_auto_reauthn_setting_blocked,
    bool is_auto_reauthn_embargoed,
    std::optional<base::TimeDelta> time_from_embargo,
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

void FedCmMetrics::RecordAccountsDialogShown(
    const std::vector<IdentityProviderData>& providers) {
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetAccountsDialogShown(true);
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);

  for (const auto& provider : providers) {
    ukm::builders::Blink_FedCmIdp fedcm_idp_builder(
        GetOrCreateProviderSourceId(provider.idp_metadata.config_url));
    // A provider may have no accounts and be present due to IDP mismatch.
    if (!provider.accounts.empty()) {
      RecordUkm(fedcm_idp_builder);
    } else {
      DCHECK(provider.has_login_status_mismatch);
      fedcm_idp_builder.SetMismatchDialogShown(true);
      fedcm_idp_builder.SetFedCmSessionID(session_id_);
      fedcm_idp_builder.Record(ukm::UkmRecorder::Get());
    }
  }

  base::UmaHistogramBoolean("Blink.FedCm.AccountsDialogShown", true);
}

void FedCmMetrics::RecordMismatchDialogShown(bool has_shown_mismatch,
                                             bool has_hints) {
  MismatchDialogType type;
  if (!has_shown_mismatch) {
    type = has_hints ? MismatchDialogType::kFirstWithHints
                     : MismatchDialogType::kFirstWithoutHints;
  } else {
    type = has_hints ? MismatchDialogType::kRepeatedWithHints
                     : MismatchDialogType::kRepeatedWithoutHints;
  }
  base::UmaHistogramEnumeration("Blink.FedCm.MismatchDialogType", type);

  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetMismatchDialogShown(true);
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);

  ukm::builders::Blink_FedCmIdp fedcm_idp_builder((provider_source_id_));
  RecordUkm(fedcm_idp_builder);

  base::UmaHistogramBoolean("Blink.FedCm.MismatchDialogShown", true);
}

void FedCmMetrics::RecordAccountsRequestSent() {
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

void FedCmMetrics::RecordDisconnectMetrics(
    FedCmDisconnectStatus status,
    std::optional<base::TimeDelta> duration,
    const RenderFrameHost& rfh,
    url::Origin requester,
    url::Origin embedder) {
  FedCmRequesterFrameType requester_frame_type =
      ComputeRequesterFrameType(rfh, requester, embedder);
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetStatus_Disconnect(static_cast<int>(status));
    ukm_builder.SetDisconnect_FrameType(static_cast<int>(requester_frame_type));
    if (duration) {
      ukm_builder.SetTiming_Disconnect(
          ukm::GetSemanticBucketMinForDurationTiming(
              duration->InMilliseconds()));
    }
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);

  ukm::builders::Blink_FedCmIdp fedcm_idp_builder(provider_source_id_);
  RecordUkm(fedcm_idp_builder);

  base::UmaHistogramEnumeration("Blink.FedCm.Status.Disconnect", status);
  base::UmaHistogramEnumeration("Blink.FedCm.Disconnect.FrameType",
                                requester_frame_type);
  if (duration) {
    base::UmaHistogramMediumTimes("Blink.FedCm.Timing.Disconnect", *duration);
  }
}

void FedCmMetrics::RecordErrorDialogResult(FedCmErrorDialogResult result) {
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

void FedCmMetrics::RecordMultipleRequestsRpMode(
    blink::mojom::RpMode pending_request_rp_mode,
    blink::mojom::RpMode new_request_rp_mode) {
  FedCmMultipleRequestsRpMode status;
  if (pending_request_rp_mode == blink::mojom::RpMode::kWidget) {
    status = new_request_rp_mode == blink::mojom::RpMode::kWidget
                 ? FedCmMultipleRequestsRpMode::kWidgetThenWidget
                 : FedCmMultipleRequestsRpMode::kWidgetThenButton;
  } else {
    status = new_request_rp_mode == blink::mojom::RpMode::kWidget
                 ? FedCmMultipleRequestsRpMode::kButtonThenWidget
                 : FedCmMultipleRequestsRpMode::kButtonThenButton;
  }
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetMultipleRequestsRpMode(static_cast<int>(status));
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);

  ukm::builders::Blink_FedCmIdp fedcm_idp_builder(provider_source_id_);
  RecordUkm(fedcm_idp_builder);

  base::UmaHistogramEnumeration("Blink.FedCm.MultipleRequestsRpMode", status);
}

void FedCmMetrics::RecordTimeBetweenUserInfoAndButtonModeAPI(
    base::TimeDelta duration) {
  auto RecordUkm = [&](auto& ukm_builder) {
    ukm_builder.SetTiming_GetUserInfoToButtonMode(
        ukm::GetExponentialBucketMinForUserTiming(duration.InMilliseconds()));
    ukm_builder.SetFedCmSessionID(session_id_);
    ukm_builder.Record(ukm::UkmRecorder::Get());
  };
  ukm::builders::Blink_FedCm fedcm_builder(page_source_id_);
  RecordUkm(fedcm_builder);

  base::UmaHistogramMediumTimes("Blink.FedCm.Timing.GetUserInfoToButtonMode",
                                duration);
}

ukm::SourceId FedCmMetrics::GetOrCreateProviderSourceId(const GURL& provider) {
  auto it = provider_source_ids_.find(provider);
  if (it != provider_source_ids_.end()) {
    return it->second;
  }
  ukm::SourceId source_id =
      ukm::UkmRecorder::GetSourceIdForWebIdentityFromScope(
          base::PassKey<FedCmMetrics>(), provider);
  provider_source_ids_[provider] = source_id;
  return source_id;
}

void RecordPreventSilentAccess(RenderFrameHost& rfh,
                               url::Origin requester,
                               url::Origin embedder) {
  FedCmRequesterFrameType requester_frame_type =
      ComputeRequesterFrameType(rfh, requester, embedder);
  base::UmaHistogramEnumeration("Blink.FedCm.PreventSilentAccessFrameType",
                                requester_frame_type);

  // Ensure the lifecycle state as GetPageUkmSourceId doesn't support the
  // prerendering page. As FederatedAithRequest runs behind the
  // BrowserInterfaceBinders, the service doesn't receive any request while
  // prerendering, and the CHECK should always meet the condition.
  CHECK(
      !rfh.IsInLifecycleState(RenderFrameHost::LifecycleState::kPrerendering));
  ukm::builders::Blink_FedCm ukm_builder(rfh.GetPageUkmSourceId());
  ukm_builder.SetPreventSilentAccessFrameType(
      static_cast<int>(requester_frame_type));
  ukm_builder.Record(ukm::UkmRecorder::Get());
}

void RecordApprovedClientsExistence(bool has_approved_clients) {
  base::UmaHistogramBoolean("Blink.FedCm.ApprovedClientsExistence",
                            has_approved_clients);
}

void RecordApprovedClientsSize(int size) {
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

void RecordLifecycleStateFailureReason(
    FedCmLifecycleStateFailureReason reason) {
  base::UmaHistogramEnumeration("Blink.FedCm.LifecycleStateFailureReason",
                                reason);
}

void RecordRawAccountsSize(int size) {
  CHECK_GT(size, 0);
  base::UmaHistogramCustomCounts("Blink.FedCm.AccountsSize.Raw", size,
                                 /*min=*/1,
                                 /*max=*/10, /*buckets=*/10);
}

void RecordReadyToShowAccountsSize(int size) {
  CHECK_GT(size, 0);
  base::UmaHistogramCustomCounts("Blink.FedCm.AccountsSize.ReadyToShow", size,
                                 /*min=*/1,
                                 /*max=*/10, /*buckets=*/10);
}

}  // namespace content
