// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/digital_credentials/digital_identity_request_impl.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/barrier_callback.h"
#include "base/base64url.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "base/values.h"
#include "content/browser/webid/delegation/sd_jwt.h"
#include "content/browser/webid/flags.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/digital_identity_interstitial_type.h"
#include "content/public/browser/digital_identity_provider.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "services/network/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "third_party/blink/public/mojom/webid/digital_identity_request.mojom-forward.h"
#include "third_party/re2/src/re2/re2.h"

using blink::mojom::RequestDigitalIdentityStatus;
using InterstitialType = content::DigitalIdentityInterstitialType;
using RequestStatusForMetrics =
    content::DigitalIdentityProvider::RequestStatusForMetrics;
using DigitalIdentityInterstitialAbortCallback =
    content::DigitalIdentityProvider::DigitalIdentityInterstitialAbortCallback;

namespace content {
namespace {
namespace sdjwt = ::content::sdjwt;

constexpr char kOpenid4vpProtocolPrefix[] = "openid4vp";

constexpr char kMdocAgeOverDataElementRegex[] = R"(age_over_\d\d)";
constexpr char kMdocAgeInYearsDataElement[] = "age_in_years";
constexpr char kMdocAgeBirthYearDataElement[] = "age_birth_year";
constexpr char kMdocBirthDateDataElement[] = "birth_date";

constexpr char kSubscriptionHint[] = "subscription_hint";
constexpr char kCarrierHint[] = "carrier_hint";
constexpr char kAndroidCarrierHint[] = "android_carrier_hint";

constexpr char kGetPhoneNumberVctValue[] =
    "number-verification/device-phone-number/ts43";
constexpr char kVerifyPhoneNumberVctValue[] = "number-verification/verify/ts43";

constexpr char kDpcVctValue[] = "com.emvco.dpc";
constexpr char kDpcCredCardVctValue[] = "dpc.cred.card";

constexpr char kDcqlQuery[] = "dcql_query";
constexpr char kMdocFormat[] = "mso_mdoc";
constexpr char kMdlDocumentType[] = "org.iso.18013.5.1.mDL";
constexpr char kMdlNamespace[] = "org.iso.18013.5.1";
constexpr char kMeta[] = "meta";
constexpr char kDoctypeValue[] = "doctype_value";

constexpr char kDigitalIdentityDialogParam[] = "dialog";
constexpr char kDigitalIdentityNoDialogParamValue[] = "no_dialog";
constexpr char kDigitalIdentityLowRiskDialogParamValue[] = "low_risk";
constexpr char kDigitalIdentityHighRiskDialogParamValue[] = "high_risk";

// Returns whether an interstitial should be shown for a request which solely
// requests the passed-in claims/data elements.
bool CanClaimBypassInterstitial(const std::string& claim) {
  if (re2::RE2::FullMatch(claim, re2::RE2(kMdocAgeOverDataElementRegex))) {
    return true;
  }

  const std::string kClaimsCanBypassInterstitial[] = {
      kMdocAgeInYearsDataElement,
      kMdocAgeBirthYearDataElement,
      kMdocBirthDateDataElement,
      kSubscriptionHint,
      kCarrierHint,
      kAndroidCarrierHint,
  };
  return std::find(std::begin(kClaimsCanBypassInterstitial),
                   std::end(kClaimsCanBypassInterstitial),
                   claim) != std::end(kClaimsCanBypassInterstitial);
}

// Returns whether the vct value is a Digital Payment Credential (DPC).
bool IsDpcVctValue(const std::string& vct_value) {
  return vct_value == kDpcVctValue || vct_value == kDpcCredCardVctValue;
}

// Returns whether the doctype value is a Digital Payment Credential (DPC).
bool IsDpcDocTypeValue(const std::string& doctype_value) {
  return doctype_value == kDpcVctValue;
}

bool CanVctValueBypassInterstitial(const std::string& vct_value) {
  return vct_value == kGetPhoneNumberVctValue ||
         vct_value == kVerifyPhoneNumberVctValue;
}


// Returns whether the request is a Digital Payment Credential (DPC) request
// that can bypass the interstitial.
bool IsDpcRequest(const base::flat_set<std::string>& all_claims,
                  const base::flat_set<std::string>& all_vct_values,
                  const base::flat_set<std::string>& all_doctype_values) {
  // A DPC request is identified by either a DPC vct_value or a DPC
  // doctype_value.
  bool has_dpc_indicator =
      std::ranges::any_of(all_vct_values, IsDpcVctValue) ||
      std::ranges::any_of(all_doctype_values, IsDpcDocTypeValue);
  if (!has_dpc_indicator) {
    return false;
  }
  // Even for DPC, the interstitial is only bypassed if no sensitive claims are
  // requested.
  return std::ranges::all_of(all_claims, CanClaimBypassInterstitial);
}

bool CanRequestCredentialBypassInterstitialForOpenid4vpProtocolWithDCQL(
    const base::DictValue& request) {
  const base::DictValue* query_dict = request.FindDict(kDcqlQuery);
  if (!query_dict) {
    return false;
  }
  auto credential_to_claims =
      [](const base::DictValue& credential) -> std::vector<std::string> {
    const base::ListValue* claims_list = credential.FindList("claims");
    if (!claims_list) {
      return {};
    }

    const std::string* format = credential.FindString("format");
    const base::DictValue* meta = credential.FindDict(kMeta);
    const std::string* doctype =
        meta ? meta->FindString(kDoctypeValue) : nullptr;

    bool is_mdl = format && *format == kMdocFormat && doctype &&
                  *doctype == kMdlDocumentType;

    std::vector<std::string> claims;
    for (const base::Value& claim : *claims_list) {
      const base::DictValue* claim_dict = claim.GetIfDict();
      if (!claim_dict) {
        return {};
      }
      const base::ListValue* paths = claim_dict->FindList("path");
      if (!paths || paths->empty()) {
        return {};
      }
      const std::string* claim_name = paths->back().GetIfString();
      if (!claim_name) {
        return {};
      }

      bool path_ok = paths->size() == 2u && paths->front().is_string() &&
                     paths->front().GetString() == kMdlNamespace;

      bool is_mdl_claim =
          re2::RE2::FullMatch(*claim_name,
                              re2::RE2(kMdocAgeOverDataElementRegex)) ||
          *claim_name == kMdocAgeInYearsDataElement ||
          *claim_name == kMdocAgeBirthYearDataElement ||
          *claim_name == kMdocBirthDateDataElement;

      if (is_mdl_claim) {
        if (is_mdl && path_ok) {
          claims.push_back(*claim_name);
        } else {
          claims.push_back("__invalid_context__");
        }
      } else {
        claims.push_back(*claim_name);
      }
    }
    return claims;
  };

  auto meta_to_vct_values =
      [](const base::DictValue& meta) -> std::vector<std::string> {
    const base::ListValue* vct_values_list = meta.FindList("vct_values");
    if (!vct_values_list) {
      return {};
    }
    std::vector<std::string> vct_values;
    for (const base::Value& vct_value : *vct_values_list) {
      if (vct_value.is_string()) {
        vct_values.push_back(vct_value.GetString());
      }
    }
    return vct_values;
  };

  auto meta_to_doctype_value = [](const base::DictValue& meta) -> std::string {
    const std::string* doctype_value = meta.FindString(kDoctypeValue);
    return doctype_value ? *doctype_value : "";
  };

  const base::ListValue* credentials = query_dict->FindList("credentials");
  if (!credentials) {
    return false;
  }

  base::flat_set<std::string> all_claims;
  base::flat_set<std::string> all_vct_values;
  base::flat_set<std::string> all_doctype_values;
  for (const base::Value& credential : *credentials) {
    const base::DictValue* credential_dict = credential.GetIfDict();
    if (!credential_dict) {
      return false;
    }
    std::vector<std::string> credential_claims =
        credential_to_claims(*credential_dict);
    all_claims.insert(credential_claims.begin(), credential_claims.end());

    const base::DictValue* meta_dict = credential_dict->FindDict(kMeta);
    if (!meta_dict) {
      continue;
    }
    std::vector<std::string> meta_vct_values = meta_to_vct_values(*meta_dict);
    all_vct_values.insert(meta_vct_values.begin(), meta_vct_values.end());

    std::string doctype_value = meta_to_doctype_value(*meta_dict);
    if (!doctype_value.empty()) {
      all_doctype_values.insert(doctype_value);
    }
  }

  // The interstitial is bypassed if either:
  // 1. The request only asks for claims and vct_values that are known to be
  //    bypassable (e.g. phone number verification).
  // 2. The request is a DPC request and only asks for bypassable claims.
  return (std::ranges::all_of(all_claims, CanClaimBypassInterstitial) &&
          std::ranges::all_of(all_vct_values, CanVctValueBypassInterstitial)) ||
         IsDpcRequest(all_claims, all_vct_values, all_doctype_values);
}

bool CanRequestCredentialBypassInterstitialForOpenid4vpProtocol(
    const base::Value& request) {
  CHECK(request.is_dict());
  const base::DictValue* request_dict = &request.GetDict();

  // The request may be a JWT. In that case, we need to parse the JWT to get to
  // the actual request payload.
  std::optional<base::Value> payload;
  if (const std::string* jwt_str = request_dict->FindString("request")) {
    std::optional<base::ListValue> parsed_jwt = sdjwt::Jwt::Parse(*jwt_str);
    if (!parsed_jwt) {
      return false;
    }
    std::optional<sdjwt::Jwt> jwt = sdjwt::Jwt::From(*parsed_jwt);
    if (!jwt) {
      return false;
    }

    payload = base::JSONReader::Read(jwt->payload.value(),
                                     base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    if (!payload || !payload->is_dict()) {
      return false;
    }
    request_dict = &payload->GetDict();
  }

  if (request_dict->contains(kDcqlQuery)) {
    return CanRequestCredentialBypassInterstitialForOpenid4vpProtocolWithDCQL(
        *request_dict);
  }
  return false;
}

// Returns whether an interstitial should be shown based on the assertions being
// requested.
bool CanRequestCredentialBypassInterstitial(const std::string& protocol,
                                            const base::Value& request) {
  if (!request.is_dict()) {
    return false;
  }

  if (protocol.starts_with(kOpenid4vpProtocolPrefix)) {
    return CanRequestCredentialBypassInterstitialForOpenid4vpProtocol(request);
  }
  return false;
}

blink::mojom::RequestDigitalIdentityStatus ToRequestDigitalIdentityStatus(
    RequestStatusForMetrics status_for_metrics) {
  switch (status_for_metrics) {
    case RequestStatusForMetrics::kSuccess:
      return blink::mojom::RequestDigitalIdentityStatus::kSuccess;
    case RequestStatusForMetrics::kErrorAborted:
      return blink::mojom::RequestDigitalIdentityStatus::kErrorCanceled;
    case RequestStatusForMetrics::kErrorNoRequests:
      return blink::mojom::RequestDigitalIdentityStatus::kErrorNoRequests;
    case RequestStatusForMetrics::kErrorNoTransientUserActivation:
      return blink::mojom::RequestDigitalIdentityStatus::
          kErrorNoTransientUserActivation;
    case RequestStatusForMetrics::kErrorOther:
      return blink::mojom::RequestDigitalIdentityStatus::kError;
    case RequestStatusForMetrics::kErrorNoCredential:
    case RequestStatusForMetrics::kErrorUserDeclined:
      return blink::mojom::RequestDigitalIdentityStatus::kErrorUserDeclined;
    case RequestStatusForMetrics::kErrorInvalidJson:
      return blink::mojom::RequestDigitalIdentityStatus::kErrorInvalidJson;
  }
}

}  // anonymous namespace

// static
base::WeakPtr<DigitalIdentityRequestImpl>
DigitalIdentityRequestImpl::CreateInstance(
    RenderFrameHost& host,
    mojo::PendingReceiver<blink::mojom::DigitalIdentityRequest> receiver) {
  // DigitalIdentityRequestImpl owns itself. It will self-destruct when a mojo
  // interface error occurs, the RenderFrameHost is deleted, or the
  // RenderFrameHost navigates to a new document.
  DigitalIdentityRequestImpl* instance =
      new DigitalIdentityRequestImpl(host, std::move(receiver));
  return instance->weak_ptr_factory_.GetWeakPtr();
}

// static
std::optional<InterstitialType>
DigitalIdentityRequestImpl::ComputeInterstitialType(
    RenderFrameHost& render_frame_host,
    const DigitalIdentityProvider* provider,
    const std::vector<blink::mojom::DigitalCredentialGetRequestPtr>&
        digital_credential_requests) {
  std::string dialog_param_value = base::GetFieldTrialParamValueByFeature(
      features::kWebIdentityDigitalCredentials, kDigitalIdentityDialogParam);
  if (dialog_param_value == kDigitalIdentityNoDialogParamValue) {
    return std::nullopt;
  }

  if (dialog_param_value == kDigitalIdentityHighRiskDialogParamValue) {
    return InterstitialType::kHighRisk;
  }

  if (dialog_param_value == kDigitalIdentityLowRiskDialogParamValue) {
    return InterstitialType::kLowRisk;
  }

  if (provider->IsLastCommittedOriginLowRisk(render_frame_host)) {
    return std::nullopt;
  }
  return std::ranges::all_of(
             digital_credential_requests,
             [](const blink::mojom::DigitalCredentialGetRequestPtr& request) {
               return CanRequestCredentialBypassInterstitial(request->protocol,
                                                             request->data);
             })
             ? std::nullopt
             : std::optional<InterstitialType>(InterstitialType::kLowRisk);
}

DigitalIdentityRequestImpl::DigitalIdentityRequestImpl(
    RenderFrameHost& host,
    mojo::PendingReceiver<blink::mojom::DigitalIdentityRequest> receiver)
    : DocumentService(host, std::move(receiver)) {}

DigitalIdentityRequestImpl::~DigitalIdentityRequestImpl() = default;

void DigitalIdentityRequestImpl::CompleteRequest(
    base::expected<DigitalIdentityProvider::DigitalCredential,
                   RequestStatusForMetrics> response) {
  RequestDigitalIdentityStatus status =
      response.has_value() ? RequestDigitalIdentityStatus::kSuccess
                           : ToRequestDigitalIdentityStatus(response.error());
  CompleteRequestWithStatus(status, std::move(response));
}

void DigitalIdentityRequestImpl::CompleteRequestWithError(
    RequestStatusForMetrics status_for_metrics) {
  CompleteRequest(base::unexpected(status_for_metrics));
}

void DigitalIdentityRequestImpl::CompleteRequestWithStatus(
    RequestDigitalIdentityStatus status,
    base::expected<DigitalIdentityProvider::DigitalCredential,
                   RequestStatusForMetrics> response) {
  // `provider_.reset()` can synchronously close UI which (via activation
  // observers) may destroy the hosting WebContents and therefore `this`.
  // Guard with a WeakPtr and bail out if that happens. Weak pointers must be
  // invalidated only after the liveness check.
  base::WeakPtr<DigitalIdentityRequestImpl> weak_this =
      weak_ptr_factory_.GetWeakPtr();
  provider_.reset();
  if (!weak_this) {
    return;
  }
  weak_ptr_factory_.InvalidateWeakPtrs();
  update_interstitial_on_abort_callback_.Reset();

  base::UmaHistogramEnumeration("Blink.DigitalIdentityRequest.Status",
                                response.has_value()
                                    ? RequestStatusForMetrics::kSuccess
                                    : response.error());

  if (response.has_value()) {
    std::move(callback_).Run(status, response->protocol,
                             std::move(response->data));
  } else {
    std::move(callback_).Run(status, std::nullopt, base::Value());
  }
}

base::Value BuildGetRequest(
    const std::vector<blink::mojom::DigitalCredentialGetRequestPtr>&
        digital_credential_requests) {
  auto requests = base::ListValue();
  for (const auto& request : digital_credential_requests) {
    auto result = base::DictValue();
    result.Set("protocol", request->protocol);
    result.Set("data", request->data.Clone());
    requests.Append(std::move(result));
  }
  base::DictValue out = base::DictValue().Set("requests", std::move(requests));
  return base::Value(std::move(out));
}

base::Value BuildCreateRequest(
    std::vector<blink::mojom::DigitalCredentialCreateRequestPtr>
        digital_credential_requests) {
  auto requests = base::ListValue();
  for (const auto& request : digital_credential_requests) {
    auto result = base::DictValue();
    result.Set("protocol", request->protocol);
    result.Set("data", std::move(request->data));
    requests.Append(std::move(result));
  }
  base::DictValue out = base::DictValue().Set("requests", std::move(requests));
  return base::Value(std::move(out));
}

void DigitalIdentityRequestImpl::Get(
    std::vector<blink::mojom::DigitalCredentialGetRequestPtr>
        digital_credential_requests,
    GetCallback callback) {
  TRACE_EVENT("content.digitalcredentials", "DigitalIdentityRequestImpl::Get",
              "size", digital_credential_requests.size());
  if (!webid::IsDigitalCredentialsEnabled()) {
    std::move(callback).Run(RequestDigitalIdentityStatus::kError,
                            /*protocol=*/std::nullopt, /*token=*/base::Value());
    return;
  }

  if (render_frame_host().IsNestedWithinFencedFrame()) {
    mojo::ReportBadMessage(
        "DigitalIdentityRequest should not be allowed in fenced frame "
        "trees.");
    return;
  }

  // Enforce Permissions Policy browser-side.
  if (!render_frame_host().IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::kDigitalCredentialsGet)) {
    ReportBadMessageAndDeleteThis(
        "digital-credentials-get permissions policy is not enabled.");
    return;
  }

  if (callback_) {
    // Only allow one in-flight wallet request.
    std::move(callback).Run(RequestDigitalIdentityStatus::kErrorTooManyRequests,
                            /*protocol=*/std::nullopt, /*token=*/base::Value());
    return;
  }

  callback_ = std::move(callback);
  if (digital_credential_requests.empty()) {
    CompleteRequestWithError(RequestStatusForMetrics::kErrorNoRequests);
    return;
  }

  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());
  if (!web_contents) {
    CompleteRequestWithError(RequestStatusForMetrics::kErrorOther);
    return;
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeUIForDigitalIdentity)) {
    // Post delayed task to enable testing abort.
    std::string fake_protocol = digital_credential_requests[0]->protocol;
    GetUIThreadTaskRunner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DigitalIdentityRequestImpl::CompleteRequest,
                       weak_ptr_factory_.GetWeakPtr(),
                       DigitalIdentityProvider::DigitalCredential(
                           fake_protocol, base::Value(base::DictValue().Set(
                                              "token", "fake_test_token")))),
        base::Milliseconds(1));
    return;
  }

  provider_ = GetContentClient()->browser()->CreateDigitalIdentityProvider();
  if (!provider_) {
    CompleteRequestWithError(RequestStatusForMetrics::kErrorOther);
    return;
  }

  if (!render_frame_host().IsActive() ||
      render_frame_host().GetVisibilityState() !=
          content::PageVisibilityState::kVisible) {
    CompleteRequestWithError(RequestStatusForMetrics::kErrorOther);
    return;
  }

  std::optional<InterstitialType> interstitial_type = ComputeInterstitialType(
      render_frame_host(), provider_.get(), digital_credential_requests);

  base::Value request_to_send = BuildGetRequest(digital_credential_requests);
  if (!interstitial_type) {
    OnInterstitialDone(std::move(request_to_send),
                       RequestStatusForMetrics::kSuccess);
    return;
  }

  update_interstitial_on_abort_callback_ =
      provider_->ShowDigitalIdentityInterstitial(
          *WebContents::FromRenderFrameHost(&render_frame_host()), origin(),
          *interstitial_type,
          base::BindOnce(&DigitalIdentityRequestImpl::OnInterstitialDone,
                         weak_ptr_factory_.GetWeakPtr(),
                         std::move(request_to_send)));
}

void DigitalIdentityRequestImpl::Create(
    std::vector<blink::mojom::DigitalCredentialCreateRequestPtr>
        digital_credential_requests,
    CreateCallback callback) {
  TRACE_EVENT("content.digitalcredentials",
              "DigitalIdentityRequestImpl::Create", "size",
              static_cast<int>(digital_credential_requests.size()));
  if (!webid::IsDigitalCredentialsCreationEnabled()) {
    std::move(callback).Run(RequestDigitalIdentityStatus::kError,
                            /*protocol=*/std::nullopt, /*token=*/base::Value());
    return;
  }

  if (render_frame_host().IsNestedWithinFencedFrame()) {
    mojo::ReportBadMessage(
        "DigitalIdentityRequest should not be allowed in fenced frame "
        "trees.");
    return;
  }

  // Enforce Permissions Policy browser-side.
  if (!render_frame_host().IsFeatureEnabled(
          network::mojom::PermissionsPolicyFeature::
              kDigitalCredentialsCreate)) {
    ReportBadMessageAndDeleteThis(
        "digital-credentials-create permissions policy is not enabled.");
    return;
  }

  if (callback_) {
    // Only allow one in-flight wallet request.
    std::move(callback).Run(RequestDigitalIdentityStatus::kErrorTooManyRequests,
                            /*protocol=*/std::nullopt, /*token=*/base::Value());
    return;
  }

  callback_ = std::move(callback);

  if (digital_credential_requests.empty()) {
    CompleteRequestWithError(RequestStatusForMetrics::kErrorNoRequests);
    return;
  }

  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());
  if (!web_contents) {
    CompleteRequestWithError(RequestStatusForMetrics::kErrorOther);
    return;
  }

  // Store the protocol to return it in tests when no digital wallet is
  // available. Pick the first one arbitrarily since it covers most of the tests
  // that send only one request.
  std::string protocol = digital_credential_requests[0]->protocol;

  base::Value request_to_send =
      BuildCreateRequest(std::move(digital_credential_requests));

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeUIForDigitalIdentity)) {
    // Post delayed task to enable testing abort+.
    GetUIThreadTaskRunner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DigitalIdentityRequestImpl::CompleteRequest,
                       weak_ptr_factory_.GetWeakPtr(),
                       DigitalIdentityProvider::DigitalCredential(
                           protocol, base::Value(base::DictValue().Set(
                                         "token", "fake_test_token")))),
        base::Milliseconds(1));
    return;
  }

  provider_ = GetContentClient()->browser()->CreateDigitalIdentityProvider();
  if (!provider_) {
    CompleteRequestWithError(RequestStatusForMetrics::kErrorOther);
    return;
  }

  if (!render_frame_host().IsActive() ||
      render_frame_host().GetVisibilityState() !=
          content::PageVisibilityState::kVisible) {
    CompleteRequestWithError(RequestStatusForMetrics::kErrorOther);
    return;
  }
  provider_->Create(WebContents::FromRenderFrameHost(&render_frame_host()),
                    origin(), request_to_send,
                    base::BindOnce(&DigitalIdentityRequestImpl::CompleteRequest,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void DigitalIdentityRequestImpl::Abort() {
  if (!callback_) {
    // Renderer sent abort request after the browser sent the callback but
    // before the renderer received it.
    return;
  }

  if (update_interstitial_on_abort_callback_) {
    std::move(update_interstitial_on_abort_callback_).Run();
  }

  CompleteRequestWithStatus(
      RequestDigitalIdentityStatus::kErrorCanceled,
      base::unexpected(RequestStatusForMetrics::kErrorAborted));
}

void DigitalIdentityRequestImpl::OnInterstitialDone(
    base::Value request_to_send,
    RequestStatusForMetrics status_after_interstitial) {
  if (status_after_interstitial != RequestStatusForMetrics::kSuccess) {
    CompleteRequestWithError(status_after_interstitial);
    return;
  }

  provider_->Get(WebContents::FromRenderFrameHost(&render_frame_host()),
                 origin(), request_to_send,
                 base::BindOnce(&DigitalIdentityRequestImpl::CompleteRequest,
                                weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace content
