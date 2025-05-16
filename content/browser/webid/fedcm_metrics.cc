// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/fedcm_metrics.h"

#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/types/pass_key.h"
#include "content/browser/webid/flags.h"
#include "content/browser/webid/webid_utils.h"
#include "net/base/net_errors.h"
#include "net/base/schemeful_site.h"
#include "net/http/http_response_headers.h"
#include "net/http/http_status_code.h"
#include "services/metrics/public/cpp/metrics_utils.h"
#include "third_party/blink/public/mojom/webid/federated_auth_request.mojom.h"
#include "url/gurl.h"

namespace content {

namespace {

FedCmMetrics::NumAccounts ComputeNumMatchingAccounts(
    size_t accounts_remaining) {
  if (accounts_remaining == 0u) {
    return FedCmMetrics::NumAccounts::kZero;
  }
  if (accounts_remaining == 1u) {
    return FedCmMetrics::NumAccounts::kOne;
  }
  return FedCmMetrics::NumAccounts::kMultiple;
}

int GetSumOfAllValues(const std::map<GURL, int>& map) {
  int total = 0;
  for (const auto& [_, value] : map) {
    total += value;
  }
  return total;
}

int GetNewSessionID() {
  return base::RandInt(1, 1 << 30);
}

}  // namespace

FedCmMetrics::FedCmMetrics(ukm::SourceId page_source_id)
    : page_source_id_(page_source_id) {
  session_id_ = GetNewSessionID();
}

FedCmMetrics::~FedCmMetrics() {
  if (fedcm_builder_) {
    fedcm_builder_->SetFedCmSessionID(session_id_)
        .Record(ukm::UkmRecorder::Get());
  }
  for (auto& [_, builder] : provider_to_fedcm_idp_builder_) {
    builder->SetFedCmSessionID(session_id_).Record(ukm::UkmRecorder::Get());
  }
}

ukm::builders::Blink_FedCm* FedCmMetrics::GetOrCreateFedCmBuilder() {
  if (!fedcm_builder_) {
    fedcm_builder_ =
        std::make_unique<ukm::builders::Blink_FedCm>(page_source_id_);
  }
  return fedcm_builder_.get();
}

ukm::builders::Blink_FedCmIdp* FedCmMetrics::GetOrCreateFedCmIdpBuilder(
    const GURL& provider) {
  if (auto it = provider_to_fedcm_idp_builder_.find(provider);
      it != provider_to_fedcm_idp_builder_.end()) {
    return it->second.get();
  }

  std::unique_ptr<ukm::builders::Blink_FedCmIdp> fedcm_idp_builder =
      std::make_unique<ukm::builders::Blink_FedCmIdp>(
          GetOrCreateProviderSourceId(provider));
  return provider_to_fedcm_idp_builder_
      .insert({provider, std::move(fedcm_idp_builder)})
      .first->second.get();
}

void FedCmMetrics::RecordShowAccountsDialogTime(
    const std::vector<IdentityProviderDataPtr>& providers,
    base::TimeDelta duration) {
  auto SetUkm = [&](auto ukm_builder) {
    ukm_builder->SetTiming_ShowAccountsDialog(
        ukm::GetExponentialBucketMinForUserTiming(duration.InMilliseconds()));
  };

  SetUkm(GetOrCreateFedCmBuilder());
  for (const auto& provider : providers) {
    // A provider may have no accounts, for instance if present due to IDP
    // mismatch.
    if (!provider->has_login_status_mismatch) {
      SetUkm(GetOrCreateFedCmIdpBuilder(provider->idp_metadata.config_url));
    }
  }

  base::UmaHistogramMediumTimes("Blink.FedCm.Timing.ShowAccountsDialog",
                                duration);
}

void FedCmMetrics::RecordShowAccountsDialogTimeBreakdown(
    base::TimeDelta well_known_and_config_fetch_duration,
    base::TimeDelta accounts_fetch_duration,
    base::TimeDelta client_metadata_fetch_duration) {
  auto SetUkm = [&](auto ukm_builder) {
    ukm_builder->SetTiming_ShowAccountsDialogBreakdown_WellKnownAndConfigFetch(
        ukm::GetExponentialBucketMinForUserTiming(
            well_known_and_config_fetch_duration.InMilliseconds()));
    ukm_builder->SetTiming_ShowAccountsDialogBreakdown_AccountsFetch(
        ukm::GetExponentialBucketMinForUserTiming(
            accounts_fetch_duration.InMilliseconds()));
    ukm_builder->SetTiming_ShowAccountsDialogBreakdown_ClientMetadataFetch(
        ukm::GetExponentialBucketMinForUserTiming(
            client_metadata_fetch_duration.InMilliseconds()));
  };

  SetUkm(GetOrCreateFedCmBuilder());

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
  ukm::builders::Blink_FedCm ukm_builder(page_source_id);
  ukm_builder.SetNumRequestsPerDocument(num_requests);
  ukm_builder.Record(ukm::UkmRecorder::Get());

  base::UmaHistogramCounts100("Blink.FedCm.NumRequestsPerDocument",
                              num_requests);
}

void FedCmMetrics::RecordContinueOnPopupTime(const GURL& provider,
                                             base::TimeDelta duration) {
  auto SetUkm = [&](auto ukm_builder) {
    ukm_builder->SetTiming_ContinueOnDialog(
        ukm::GetExponentialBucketMinForUserTiming(duration.InMilliseconds()));
  };

  SetUkm(GetOrCreateFedCmBuilder());
  SetUkm(GetOrCreateFedCmIdpBuilder(provider));

  base::UmaHistogramMediumTimes("Blink.FedCm.Timing.ContinueOnDialog",
                                duration);
}

void FedCmMetrics::RecordCancelOnDialogTime(
    const std::vector<IdentityProviderDataPtr>& providers,
    base::TimeDelta duration) {
  auto SetUkm = [&](auto ukm_builder) {
    ukm_builder->SetTiming_CancelOnDialog(
        ukm::GetExponentialBucketMinForUserTiming(duration.InMilliseconds()));
  };

  SetUkm(GetOrCreateFedCmBuilder());
  for (const auto& provider : providers) {
    SetUkm(GetOrCreateFedCmIdpBuilder(provider->idp_metadata.config_url));
  }

  base::UmaHistogramMediumTimes("Blink.FedCm.Timing.CancelOnDialog", duration);
}

void FedCmMetrics::RecordAccountsDialogShownDuration(
    const std::vector<IdentityProviderDataPtr>& providers,
    base::TimeDelta duration) {
  auto SetUkm = [&](auto ukm_builder) {
    ukm_builder->SetTiming_AccountsDialogShownDuration(
        ukm::GetExponentialBucketMinForUserTiming(duration.InMilliseconds()));
  };

  SetUkm(GetOrCreateFedCmBuilder());
  for (const auto& provider : providers) {
    ukm::builders::Blink_FedCmIdp* fedcm_idp_builder =
        GetOrCreateFedCmIdpBuilder(provider->idp_metadata.config_url);
    // A provider may have no accounts and be present due to IDP mismatch.
    if (!provider->has_login_status_mismatch) {
      SetUkm(fedcm_idp_builder);
    } else {
      fedcm_idp_builder->SetTiming_MismatchDialogShownDuration(
          ukm::GetExponentialBucketMinForUserTiming(duration.InMilliseconds()));
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
    const std::vector<IdentityProviderDataPtr>& providers,
    base::TimeDelta duration) {
  auto SetUkm = [&](auto ukm_builder) {
    ukm_builder->SetTiming_MismatchDialogShownDuration(
        ukm::GetExponentialBucketMinForUserTiming(duration.InMilliseconds()));
  };

  SetUkm(GetOrCreateFedCmBuilder());
  for (const auto& provider : providers) {
    // We should only reach this if all `providers` are a mismatch.
    SetUkm(GetOrCreateFedCmIdpBuilder(provider->idp_metadata.config_url));
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
  auto SetUkm = [&](auto ukm_builder) {
    ukm_builder->SetCancelReason(
        static_cast<std::underlying_type_t<
            IdentityRequestDialogController::DismissReason>>(dismiss_reason));
  };
  SetUkm(GetOrCreateFedCmBuilder());

  base::UmaHistogramEnumeration("Blink.FedCm.CancelReason", dismiss_reason);
}

void FedCmMetrics::RecordTokenResponseAndTurnaroundTime(
    const GURL& provider,
    base::TimeDelta token_response_time,
    base::TimeDelta turnaround_time) {
  auto SetUkm = [&](auto ukm_builder) {
    ukm_builder
        ->SetTiming_IdTokenResponse(ukm::GetExponentialBucketMinForUserTiming(
            token_response_time.InMilliseconds()))
        .SetTiming_TurnaroundTime(ukm::GetExponentialBucketMinForUserTiming(
            turnaround_time.InMilliseconds()));
  };

  SetUkm(GetOrCreateFedCmBuilder());
  SetUkm(GetOrCreateFedCmIdpBuilder(provider));

  base::UmaHistogramMediumTimes("Blink.FedCm.Timing.IdTokenResponse",
                                token_response_time);
  base::UmaHistogramMediumTimes("Blink.FedCm.Timing.TurnaroundTime",
                                turnaround_time);
}

void FedCmMetrics::RecordContinueOnResponseAndTurnaroundTime(
    base::TimeDelta token_response_time,
    base::TimeDelta turnaround_time) {
  base::UmaHistogramMediumTimes("Blink.FedCm.Timing.ContinueOn.Response",
                                token_response_time);
  base::UmaHistogramMediumTimes("Blink.FedCm.Timing.ContinueOn.TurnaroundTime",
                                turnaround_time);
}

void FedCmMetrics::RecordRequestTokenStatus(
    FedCmRequestIdTokenStatus status,
    MediationRequirement requirement,
    const std::vector<GURL>& requested_providers,
    int num_idps_mismatch,
    const std::optional<GURL>& selected_idp_config_url,
    const RpMode& rp_mode,
    std::optional<FedCmUseOtherAccountResult> use_other_account_result,
    std::optional<FedCmVerifyingDialogResult> verifying_dialog_result,
    FedCmThirdPartyCookiesStatus tpc_status,
    const FedCmRequesterFrameType& requester_frame_type,
    std::optional<bool> has_signin_account) {
  // The following check is to avoid double recording in the following scenario:
  // 1. The request has failed but we have not yet rejected the promise, e.g.
  // when the API is disabled. We record a metric immediately but only post a
  // task to later reject the callback.
  // 2. The page is unloaded. This invokes the FederatedAuthRequestImpl
  // destructor. We record a metric with unhandled status since the callback is
  // still present.
  if (has_recorded_request_token_status_) {
    return;
  }

  // Use exponential bucketing to log these numbers.
  num_idps_mismatch =
      ukm::GetExponentialBucketMinForCounts1000(num_idps_mismatch);
  int num_idps_requested =
      ukm::GetExponentialBucketMinForCounts1000(requested_providers.size());

  auto SetUkm = [&](auto ukm_builder, FedCmRequestIdTokenStatus ukm_status) {
    ukm_builder->SetStatus_RequestIdToken(static_cast<int>(ukm_status));
    ukm_builder->SetStatus_MediationRequirement(static_cast<int>(requirement));
    ukm_builder->SetNumIdpsRequested(num_idps_requested);
    ukm_builder->SetNumIdpsMismatch(num_idps_mismatch);
    ukm_builder->SetRpMode(static_cast<int>(rp_mode));
    ukm_builder->SetThirdPartyCookiesStatus(
        static_cast<std::underlying_type_t<FedCmThirdPartyCookiesStatus>>(
            tpc_status));
    if (use_other_account_result.has_value()) {
      ukm_builder->SetUseOtherAccountResult(
          static_cast<int>(*use_other_account_result));
    }
    if (verifying_dialog_result.has_value()) {
      ukm_builder->SetVerifyingDialogResult(
          static_cast<int>(*verifying_dialog_result));
    }
    ukm_builder->SetFrameType(static_cast<int>(requester_frame_type));
    if (has_signin_account.has_value()) {
      ukm_builder->SetHasSigninAccount(*has_signin_account);
    }
  };

  SetUkm(GetOrCreateFedCmBuilder(), status);

  for (const auto& provider : requested_providers) {
    ukm::builders::Blink_FedCmIdp* fedcm_idp_builder =
        GetOrCreateFedCmIdpBuilder(provider);
    if (status == FedCmRequestIdTokenStatus::kSuccessUsingTokenInHttpResponse ||
        status ==
            FedCmRequestIdTokenStatus::kSuccessUsingIdentityProviderResolve) {
      CHECK(selected_idp_config_url);
      if (provider == *selected_idp_config_url) {
        SetUkm(fedcm_idp_builder, status);
      } else {
        SetUkm(fedcm_idp_builder, FedCmRequestIdTokenStatus::kOtherIdpChosen);
      }
    } else {
      SetUkm(fedcm_idp_builder, status);
    }
  }

  base::UmaHistogramEnumeration("Blink.FedCm.Status.RequestIdToken", status);
  base::UmaHistogramEnumeration("Blink.FedCm.Status.MediationRequirement",
                                requirement);
  base::UmaHistogramEnumeration("Blink.FedCm.RpMode", rp_mode);
  base::UmaHistogramEnumeration("Blink.FedCm.FrameType", requester_frame_type);
  if (use_other_account_result.has_value()) {
    base::UmaHistogramEnumeration("Blink.FedCm.UseOtherAccountResult",
                                  *use_other_account_result);
  }
  if (verifying_dialog_result.has_value()) {
    base::UmaHistogramEnumeration("Blink.FedCm.VerifyingDialogResult",
                                  *verifying_dialog_result);
  }
  if (has_signin_account.has_value()) {
    base::UmaHistogramBoolean("Blink.FedCm.HasSigninAccount",
                              *has_signin_account);
  }

  // We do not expect more request token status metrics from this API call.
  has_recorded_request_token_status_ = true;
}

void FedCmMetrics::RecordSignInStateMatchStatus(
    const GURL& provider,
    FedCmSignInStateMatchStatus status) {
  auto SetUkm = [&](auto ukm_builder) {
    ukm_builder->SetStatus_SignInStateMatch(static_cast<int>(status));
  };

  SetUkm(GetOrCreateFedCmBuilder());
  SetUkm(GetOrCreateFedCmIdpBuilder(provider));

  base::UmaHistogramEnumeration("Blink.FedCm.Status.SignInStateMatch", status);
}

// static
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
  ukm::builders::Blink_FedCm* ukm_builder = GetOrCreateFedCmBuilder();
  if (time_from_embargo) {
    // Use a custom histogram with the default number of buckets so that we set
    // the maximum to the permission embargo duration: 10 minutes. See
    // `kFederatedIdentityAutoReauthnEmbargoDuration`.
    base::UmaHistogramCustomTimes(
        "Blink.FedCm.AutoReauthn.TimeFromEmbargoWhenBlocked",
        *time_from_embargo, base::Milliseconds(10), base::Minutes(10),
        /*buckets=*/50);
    ukm_builder->SetAutoReauthn_TimeFromEmbargoWhenBlocked(
        ukm::GetExponentialBucketMinForUserTiming(
            time_from_embargo->InMilliseconds()));
  }

  if (has_single_returning_account.has_value()) {
    ukm_builder->SetAutoReauthn_ReturningAccounts(
        static_cast<int>(num_returning_accounts));
  }
  ukm_builder->SetAutoReauthn_Succeeded(auto_reauthn_success);
  ukm_builder->SetAutoReauthn_BlockedByContentSettings(
      is_auto_reauthn_setting_blocked);
  ukm_builder->SetAutoReauthn_BlockedByEmbargo(is_auto_reauthn_embargoed);
  ukm_builder->SetAutoReauthn_BlockedByPreventSilentAccess(
      requires_user_mediation);
}

void FedCmMetrics::RecordAccountsDialogShown(
    const std::vector<IdentityProviderDataPtr>& providers) {
  auto SetUkm = [&](auto ukm_builder, int accounts_dialog_shown) {
    ukm_builder->SetAccountsDialogShown2(accounts_dialog_shown);
  };

  for (const auto& provider : providers) {
    ukm::builders::Blink_FedCmIdp* fedcm_idp_builder =
        GetOrCreateFedCmIdpBuilder(provider->idp_metadata.config_url);
    // A provider may have no accounts and be present due to IDP mismatch.
    if (!provider->has_login_status_mismatch) {
      ++accounts_dialog_shown_[provider->idp_metadata.config_url];
      SetUkm(fedcm_idp_builder,
             accounts_dialog_shown_[provider->idp_metadata.config_url]);
    } else {
      DCHECK(provider->has_login_status_mismatch);
      ++mismatch_dialog_shown_[provider->idp_metadata.config_url];
      fedcm_idp_builder->SetMismatchDialogShown2(
          mismatch_dialog_shown_[provider->idp_metadata.config_url]);
    }
  }

  SetUkm(GetOrCreateFedCmBuilder(), GetSumOfAllValues(accounts_dialog_shown_));

  base::UmaHistogramBoolean("Blink.FedCm.AccountsDialogShown", true);
}

void FedCmMetrics::RecordSingleIdpMismatchDialogShown(
    const IdentityProviderData& provider,
    bool has_shown_mismatch,
    bool has_hints) {
  ++mismatch_dialog_shown_[provider.idp_metadata.config_url];
  MismatchDialogType type;
  if (!has_shown_mismatch) {
    type = has_hints ? MismatchDialogType::kFirstWithHints
                     : MismatchDialogType::kFirstWithoutHints;
  } else {
    type = has_hints ? MismatchDialogType::kRepeatedWithHints
                     : MismatchDialogType::kRepeatedWithoutHints;
  }
  base::UmaHistogramEnumeration("Blink.FedCm.MismatchDialogType", type);

  auto SetUkm = [&](auto ukm_builder, int mismatch_dialog_shown) {
    ukm_builder->SetMismatchDialogShown2(mismatch_dialog_shown);
  };

  SetUkm(GetOrCreateFedCmBuilder(), GetSumOfAllValues(mismatch_dialog_shown_));

  DCHECK(provider.has_login_status_mismatch);
  SetUkm(GetOrCreateFedCmIdpBuilder(provider.idp_metadata.config_url),
         mismatch_dialog_shown_[provider.idp_metadata.config_url]);

  base::UmaHistogramBoolean("Blink.FedCm.MismatchDialogShown", true);
}

void FedCmMetrics::RecordAccountsRequestSent(const GURL& provider_url) {
  ++accounts_request_sent_[provider_url];
  auto SetUkm = [&](auto ukm_builder, int accounts_request_sent) {
    ukm_builder->SetAccountsRequestSent2(accounts_request_sent);
  };

  SetUkm(GetOrCreateFedCmBuilder(), GetSumOfAllValues(accounts_request_sent_));
  SetUkm(GetOrCreateFedCmIdpBuilder(provider_url),
         accounts_request_sent_[provider_url]);

  base::UmaHistogramBoolean("Blink.FedCm.AccountsRequestSent", true);
}

void FedCmMetrics::RecordDisconnectMetrics(
    FedCmDisconnectStatus status,
    std::optional<base::TimeDelta> duration,
    const FedCmRequesterFrameType& requester_frame_type,
    const GURL& provider_url) {
  auto SetUkm = [&](auto ukm_builder) {
    ukm_builder->SetStatus_Disconnect(static_cast<int>(status));
    ukm_builder->SetDisconnect_FrameType(
        static_cast<int>(requester_frame_type));
    if (duration) {
      ukm_builder->SetTiming_Disconnect(
          ukm::GetSemanticBucketMinForDurationTiming(
              duration->InMilliseconds()));
    }
  };

  SetUkm(GetOrCreateFedCmBuilder());
  SetUkm(GetOrCreateFedCmIdpBuilder(provider_url));

  base::UmaHistogramEnumeration("Blink.FedCm.Status.Disconnect", status);
  base::UmaHistogramEnumeration("Blink.FedCm.Disconnect.FrameType",
                                requester_frame_type);
  if (duration) {
    base::UmaHistogramMediumTimes("Blink.FedCm.Timing.Disconnect", *duration);
  }
}

void FedCmMetrics::RecordContinueOnPopupStatus(
    FedCmContinueOnPopupStatus status) {
  base::UmaHistogramEnumeration("Blink.FedCm.ContinueOn.PopupWindowStatus",
                                status);
}

void FedCmMetrics::RecordContinueOnPopupResult(
    FedCmContinueOnPopupResult result) {
  base::UmaHistogramEnumeration("Blink.FedCm.ContinueOn.PopupWindowResult",
                                result);
}

void FedCmMetrics::RecordRpParameters(FedCmRpParameters parameters) {
  base::UmaHistogramEnumeration("Blink.FedCm.RpParametersAndScopeState",
                                parameters);
}

void FedCmMetrics::RecordErrorDialogResult(FedCmErrorDialogResult result,
                                           const GURL& provider_url) {
  auto SetUkm = [&](auto ukm_builder) {
    ukm_builder->SetError_ErrorDialogResult(static_cast<int>(result));
  };

  SetUkm(GetOrCreateFedCmBuilder());
  SetUkm(GetOrCreateFedCmIdpBuilder(provider_url));

  base::UmaHistogramEnumeration("Blink.FedCm.Error.ErrorDialogResult", result);
}

void FedCmMetrics::RecordErrorMetricsBeforeShowingErrorDialog(
    IdpNetworkRequestManager::FedCmTokenResponseType response_type,
    std::optional<IdpNetworkRequestManager::FedCmErrorDialogType> dialog_type,
    std::optional<IdpNetworkRequestManager::FedCmErrorUrlType> url_type,
    const GURL& provider_url) {
  auto SetUkm = [&](auto ukm_builder) {
    ukm_builder->SetError_TokenResponseType(static_cast<int>(response_type));
    if (dialog_type) {
      ukm_builder->SetError_ErrorDialogType(static_cast<int>(*dialog_type));
    }
  };

  SetUkm(GetOrCreateFedCmBuilder());

  ukm::builders::Blink_FedCmIdp* fedcm_idp_builder =
      GetOrCreateFedCmIdpBuilder(provider_url);
  // We do not record the RP-keyed equivalent because the error URL is passed by
  // the IDP.
  if (url_type) {
    fedcm_idp_builder->SetError_ErrorUrlType(static_cast<int>(*url_type));
  }
  SetUkm(fedcm_idp_builder);

  base::UmaHistogramEnumeration("Blink.FedCm.Error.TokenResponseType",
                                response_type);
  if (dialog_type) {
    base::UmaHistogramEnumeration("Blink.FedCm.Error.ErrorDialogType",
                                  *dialog_type);
  }
  if (url_type) {
    base::UmaHistogramEnumeration("Blink.FedCm.Error.ErrorUrlType", *url_type);
  }
}

void FedCmMetrics::RecordMultipleRequestsRpMode(
    blink::mojom::RpMode pending_request_rp_mode,
    blink::mojom::RpMode new_request_rp_mode,
    const std::vector<GURL>& requested_providers) {
  FedCmMultipleRequestsRpMode status;
  if (pending_request_rp_mode == blink::mojom::RpMode::kPassive) {
    status = new_request_rp_mode == blink::mojom::RpMode::kPassive
                 ? FedCmMultipleRequestsRpMode::kPassiveThenPassive
                 : FedCmMultipleRequestsRpMode::kPassiveThenActive;
  } else {
    status = new_request_rp_mode == blink::mojom::RpMode::kPassive
                 ? FedCmMultipleRequestsRpMode::kActiveThenPassive
                 : FedCmMultipleRequestsRpMode::kActiveThenActive;
  }
  auto SetUkm = [&](auto ukm_builder) {
    ukm_builder->SetMultipleRequestsRpMode(static_cast<int>(status));
  };

  SetUkm(GetOrCreateFedCmBuilder());
  for (const auto& provider : requested_providers) {
    SetUkm(GetOrCreateFedCmIdpBuilder(provider));
  }

  base::UmaHistogramEnumeration("Blink.FedCm.MultipleRequestsRpMode", status);
}

void FedCmMetrics::RecordTimeBetweenUserInfoAndActiveModeAPI(
    base::TimeDelta duration) {
  auto SetUkm = [&](auto ukm_builder) {
    ukm_builder->SetTiming_GetUserInfoToButtonMode(
        ukm::GetExponentialBucketMinForUserTiming(duration.InMilliseconds()));
  };

  SetUkm(GetOrCreateFedCmBuilder());

  base::UmaHistogramMediumTimes("Blink.FedCm.Timing.GetUserInfoToButtonMode",
                                duration);
}

void FedCmMetrics::RecordNumMatchingAccounts(size_t accounts_remaining,
                                             const std::string& filter_type) {
  FedCmMetrics::NumAccounts num_matching =
      ComputeNumMatchingAccounts(accounts_remaining);
  base::UmaHistogramEnumeration(
      "Blink.FedCm." + filter_type + ".NumMatchingAccounts", num_matching);

  ukm::builders::Blink_FedCm* fedcm_builder = GetOrCreateFedCmBuilder();
  if (filter_type == "LoginHint") {
    fedcm_builder->SetLoginHint_NumMatchingAccounts(
        static_cast<int>(num_matching));
  } else if (filter_type == "AccountLabel") {
    fedcm_builder->SetAccountLabel_NumMatchingAccounts(
        static_cast<int>(num_matching));
  } else if (filter_type == "DomainHint") {
    fedcm_builder->SetDomainHint_NumMatchingAccounts(
        static_cast<int>(num_matching));
  } else {
    NOTREACHED();
  }
}

void FedCmMetrics::RecordMultipleRequestsFromDifferentIdPs(
    bool from_different_idps) {
  auto SetUkm = [&](auto ukm_builder) {
    ukm_builder->SetMultipleRequestsFromDifferentIdPs(from_different_idps);
  };

  SetUkm(GetOrCreateFedCmBuilder());

  base::UmaHistogramBoolean("Blink.FedCm.MultipleRequestsFromDifferentIdPs",
                            from_different_idps);
}

void FedCmMetrics::RecordRpUrlHasPath(bool rp_url_has_path) {
  auto SetUkm = [&](auto ukm_builder) {
    ukm_builder->SetRpUrlHasPath(rp_url_has_path);
  };

  SetUkm(GetOrCreateFedCmBuilder());
}

void FedCmMetrics::RecordIdentityProvidersCount(int count) {
  CHECK_GT(count, 0);
  base::UmaHistogramCounts100("Blink.FedCm.IdentityProvidersCount", count);
  auto SetUkm = [&](auto ukm_builder) {
    ukm_builder->SetIdentityProvidersCount(
        ukm::GetExponentialBucketMin(count, /*bucket_spacing=*/1.3));
  };

  SetUkm(GetOrCreateFedCmBuilder());
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

int FedCmMetrics::GetSessionID() const {
  return session_id_;
}

void RecordPreventSilentAccess(
    const FedCmRequesterFrameType& requester_frame_type,
    int source_id) {
  base::UmaHistogramEnumeration("Blink.FedCm.PreventSilentAccessFrameType",
                                requester_frame_type);

  ukm::builders::Blink_FedCm ukm_builder(source_id);
  ukm_builder.SetPreventSilentAccessFrameType(
      static_cast<int>(requester_frame_type));
  ukm_builder.Record(ukm::UkmRecorder::Get());
}

void RecordAccountSelectionScrollPosition(int source_id,
                                          int session_id,
                                          const gfx::Point& scroll_position) {
  ukm::builders::Blink_FedCm ukm_builder(source_id);
  ukm_builder.SetAccountSelectionScrollPosition(ukm::GetExponentialBucketMin(
      scroll_position.y(), /*bucket_spacing=*/1.15));
  ukm_builder.SetFedCmSessionID(session_id);
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
                                 /*exclusive_max=*/10, /*buckets=*/10);
}

void RecordReadyToShowAccountsSize(int size) {
  CHECK_GT(size, 0);
  base::UmaHistogramCustomCounts("Blink.FedCm.AccountsSize.ReadyToShow", size,
                                 /*min=*/1,
                                 /*exclusive_max=*/10, /*buckets=*/10);
}

}  // namespace content
