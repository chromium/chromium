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
#include "content/browser/digital_credentials/digital_credential_environment.h"
#include "content/browser/digital_credentials/virtual_wallet.h"
#include "content/browser/renderer_host/frame_tree_node.h"
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
    bool is_sdjwt = format && (*format == "dc+sd-jwt" ||
                               *format == "dc-authorization+sd-jwt");

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

      bool path_ok = false;
      if (is_mdl) {
        path_ok = paths->size() == 2u && paths->front().is_string() &&
                  paths->front().GetString() == kMdlNamespace;
      } else if (is_sdjwt) {
        path_ok = paths->size() == 1u;
      }

      bool is_mdl_claim =
          re2::RE2::FullMatch(*claim_name,
                              re2::RE2(kMdocAgeOverDataElementRegex)) ||
          *claim_name == kMdocAgeInYearsDataElement ||
          *claim_name == kMdocAgeBirthYearDataElement ||
          *claim_name == kMdocBirthDateDataElement;

      if (is_mdl_claim) {
        if ((is_mdl || is_sdjwt) && path_ok) {
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

  for (const base::Value& credential : *credentials) {
    const base::DictValue* credential_dict = credential.GetIfDict();
    if (!credential_dict) {
      return false;
    }

    std::vector<std::string> claims = credential_to_claims(*credential_dict);
    if (!std::ranges::all_of(claims, CanClaimBypassInterstitial)) {
      return false;
    }

    const std::string* format = credential_dict->FindString("format");
    const base::DictValue* meta_dict = credential_dict->FindDict(kMeta);
    std::vector<std::string> vct_values;
    std::string doctype_value;
    if (meta_dict) {
      vct_values = meta_to_vct_values(*meta_dict);
      doctype_value = meta_to_doctype_value(*meta_dict);
    }

    bool is_dpc = std::ranges::any_of(vct_values, IsDpcVctValue) ||
                  IsDpcDocTypeValue(doctype_value);
    if (is_dpc) {
      continue;
    }

    bool is_phone =
        std::ranges::all_of(vct_values, CanVctValueBypassInterstitial);
    if (is_phone && !vct_values.empty()) {
      continue;
    }

    bool is_mdl =
        format && *format == kMdocFormat && doctype_value == kMdlDocumentType;
    if (is_mdl) {
      if (claims.empty()) {
        return false;
      }
      continue;
    }

    bool is_sdjwt = format && (*format == "dc+sd-jwt" ||
                               *format == "dc-authorization+sd-jwt");
    if (is_sdjwt) {
      if (claims.empty()) {
        return false;
      }
      continue;
    }

    // If it doesn't qualify as a DPC, MDL, SD-JWT, or phone-carrier bypass,
    // it requires an interstitial.
    return false;
  }

  return true;
}

const base::DictValue* GetOpenId4VpRequestPayload(
    const base::Value& request,
    std::optional<base::Value>& out_payload) {
  const base::DictValue* request_dict = request.GetIfDict();
  if (!request_dict) {
    return nullptr;
  }

  if (const std::string* jwt_str = request_dict->FindString("request")) {
    std::optional<base::ListValue> parsed_jwt = sdjwt::Jwt::Parse(*jwt_str);
    if (!parsed_jwt) {
      return nullptr;
    }
    std::optional<sdjwt::Jwt> jwt = sdjwt::Jwt::From(*parsed_jwt);
    if (!jwt) {
      return nullptr;
    }

    out_payload = base::JSONReader::Read(jwt->payload.value(),
                                         base::JSON_PARSE_CHROMIUM_EXTENSIONS);
    if (!out_payload) {
      return nullptr;
    }
    request_dict = out_payload->GetIfDict();
    if (!request_dict) {
      return nullptr;
    }
  }

  return request_dict;
}

bool CanRequestCredentialBypassInterstitialForOpenid4vpProtocol(
    const base::Value& request) {
  std::optional<base::Value> payload;
  const base::DictValue* request_dict =
      GetOpenId4VpRequestPayload(request, payload);
  if (!request_dict) {
    return false;
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

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(OpenId4VpResponseMode)
enum class OpenId4VpResponseMode {
  kDcApi = 0,
  kDcApiJwt = 1,
  kOther = 2,
  kMaxValue = kOther,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/blink/enums.xml:OpenId4VpResponseMode)

void RecordOpenId4VpResponseMode(const base::Value& request) {
  std::optional<base::Value> payload;
  const base::DictValue* request_dict =
      GetOpenId4VpRequestPayload(request, payload);
  if (!request_dict) {
    return;
  }

  const std::string* response_mode = request_dict->FindString("response_mode");
  if (!response_mode) {
    return;
  }

  OpenId4VpResponseMode mode = OpenId4VpResponseMode::kOther;
  if (*response_mode == "dc_api") {
    mode = OpenId4VpResponseMode::kDcApi;
  } else if (*response_mode == "dc_api.jwt") {
    mode = OpenId4VpResponseMode::kDcApiJwt;
  }
  base::UmaHistogramEnumeration(
      "Blink.DigitalIdentityRequest.OpenId4VpResponseMode", mode);
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

std::optional<VirtualWallet::Behavior>
DigitalIdentityRequestImpl::GetVirtualWalletBehavior() {
  VirtualWallet* wallet =
      DigitalCredentialEnvironment::GetInstance()->MaybeGetVirtualWallet(
          FrameTreeNode::From(&render_frame_host()));
  if (!wallet) {
    return std::nullopt;
  }
  return wallet->behavior();
}

bool DigitalIdentityRequestImpl::HandleVirtualWalletBehavior() {
  std::optional<VirtualWallet::Behavior> behavior = GetVirtualWalletBehavior();
  if (!behavior) {
    return false;
  }

  VirtualWallet* wallet =
      DigitalCredentialEnvironment::GetInstance()->MaybeGetVirtualWallet(
          FrameTreeNode::From(&render_frame_host()));

  RequestDigitalIdentityStatus status;
  std::optional<DigitalIdentityProvider::DigitalCredential> credential;

  switch (*behavior) {
    case VirtualWallet::Behavior::kRespond:
      credential = wallet->GetCredential();
      status = credential ? RequestDigitalIdentityStatus::kSuccess
                          : RequestDigitalIdentityStatus::kError;
      break;
    case VirtualWallet::Behavior::kDecline:
      status = RequestDigitalIdentityStatus::kErrorUserDeclined;
      break;
    case VirtualWallet::Behavior::kWait:
      // Leave the request's promise pending.
      return true;
  }

  GetUIThreadTaskRunner()->PostTask(
      FROM_HERE,
      base::BindOnce(&DigitalIdentityRequestImpl::CompleteRequestWithStatus,
                     weak_ptr_factory_.GetWeakPtr(), status,
                     std::move(credential)));
  return true;
}

void DigitalIdentityRequestImpl::CompleteRequest(
    base::expected<DigitalIdentityProvider::DigitalCredential,
                   RequestStatusForMetrics> response) {
  base::UmaHistogramEnumeration("Blink.DigitalIdentityRequest.Status",
                                response.has_value()
                                    ? RequestStatusForMetrics::kSuccess
                                    : response.error());

  if (response.has_value()) {
    CompleteRequestWithStatus(RequestDigitalIdentityStatus::kSuccess,
                              std::move(response.value()));
  } else {
    CompleteRequestWithStatus(ToRequestDigitalIdentityStatus(response.error()),
                              std::nullopt);
  }
}

void DigitalIdentityRequestImpl::CompleteRequestWithError(
    RequestStatusForMetrics status_for_metrics) {
  CompleteRequest(base::unexpected(status_for_metrics));
}

void DigitalIdentityRequestImpl::CompleteRequestWithStatus(
    RequestDigitalIdentityStatus status,
    std::optional<DigitalIdentityProvider::DigitalCredential> response) {
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

  if (response) {
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

  for (const auto& request : digital_credential_requests) {
    if (request->protocol.starts_with(kOpenid4vpProtocolPrefix)) {
      RecordOpenId4VpResponseMode(request->data);
    }
  }

  WebContents* web_contents =
      WebContents::FromRenderFrameHost(&render_frame_host());
  if (!web_contents) {
    CompleteRequestWithError(RequestStatusForMetrics::kErrorOther);
    return;
  }

  if (HandleVirtualWalletBehavior()) {
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

  if (HandleVirtualWalletBehavior()) {
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

  base::WeakPtr<DigitalIdentityRequestImpl> weak_this =
      weak_ptr_factory_.GetWeakPtr();
  if (update_interstitial_on_abort_callback_) {
    std::move(update_interstitial_on_abort_callback_).Run();
  }

  if (!weak_this) {
    return;
  }

  CompleteRequestWithError(RequestStatusForMetrics::kErrorAborted);
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
