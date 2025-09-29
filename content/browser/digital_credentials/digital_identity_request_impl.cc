// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/digital_credentials/digital_identity_request_impl.h"

#include <memory>
#include <optional>
#include <vector>

#include "base/barrier_callback.h"
#include "base/base64url.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webid/delegation/sd_jwt.h"
#include "content/browser/webid/flags.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/digital_identity_interstitial_type.h"
#include "content/public/browser/digital_identity_provider.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "third_party/blink/public/mojom/webid/digital_identity_request.mojom-forward.h"
#include "third_party/re2/src/re2/re2.h"

using base::Value;
using blink::mojom::RequestDigitalIdentityStatus;
using InterstitialType = content::DigitalIdentityInterstitialType;
using RequestStatusForMetrics =
    content::DigitalIdentityProvider::RequestStatusForMetrics;
using DigitalIdentityInterstitialAbortCallback =
    content::DigitalIdentityProvider::DigitalIdentityInterstitialAbortCallback;

namespace content {
namespace {
using base::Value;
namespace sdjwt = ::content::sdjwt;

constexpr char kPreviewProtocol[] = "preview";
constexpr char kOpenid4vpProtocolPrefix[] = "openid4vp";

constexpr char kMdlDocumentType[] = "org.iso.18013.5.1.mDL";

constexpr char kOpenid4vpPathRegex[] =
    R"(\$\['org\.iso\.18013\.5\.1'\]\['([^\)]*)'\])";
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

constexpr char kDigitalIdentityDialogParam[] = "dialog";
constexpr char kDigitalIdentityNoDialogParamValue[] = "no_dialog";
constexpr char kDigitalIdentityLowRiskDialogParamValue[] = "low_risk";
constexpr char kDigitalIdentityHighRiskDialogParamValue[] = "high_risk";

// Returns entry if `dict` has a list with a single dict element for key
// `list_key`.
const Value::Dict* FindSingleElementListEntry(const Value::Dict& dict,
                                              const std::string& list_key) {
  const Value::List* list = dict.FindList(list_key);
  if (!list || list->size() != 1u) {
    return nullptr;
  }
  return list->front().GetIfDict();
}

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

bool CanVctValueBypassInterstitial(const std::string& vct_value) {
  return vct_value == kGetPhoneNumberVctValue ||
         vct_value == kVerifyPhoneNumberVctValue;
}

bool CanRequestCredentialBypassInterstitialForOpenid4vpProtocolWithPresentationDefition(
    const Value::Dict& request) {
  const Value::Dict* presentation_dict =
      request.FindDict("presentation_definition");
  if (!presentation_dict) {
    return false;
  }

  const Value::Dict* input_descriptor_dict =
      FindSingleElementListEntry(*presentation_dict, "input_descriptors");
  if (!input_descriptor_dict) {
    return false;
  }

  const std::string* input_descriptor_id =
      input_descriptor_dict->FindString("id");
  if (!input_descriptor_id || *input_descriptor_id != kMdlDocumentType) {
    return false;
  }

  const Value::Dict* constraints_dict =
      input_descriptor_dict->FindDict("constraints");
  if (!constraints_dict) {
    return false;
  }

  const Value::Dict* field_dict =
      FindSingleElementListEntry(*constraints_dict, "fields");
  if (!field_dict) {
    return false;
  }

  const Value::List* field_paths = field_dict->FindList("path");
  if (!field_paths) {
    return false;
  }

  if (!field_paths || field_paths->size() != 1u ||
      !field_paths->front().is_string()) {
    return false;
  }

  std::string mdoc_data_element;
  return re2::RE2::FullMatch(field_paths->front().GetString(),
                             re2::RE2(kOpenid4vpPathRegex),
                             &mdoc_data_element) &&
         CanClaimBypassInterstitial(mdoc_data_element);
}

bool CanRequestCredentialBypassInterstitialForOpenid4vpProtocolWithDCQL(
    const Value::Dict& request) {
  const Value::Dict* query_dict = request.FindDict("dcql_query");
  if (!query_dict) {
    return false;
  }
  auto credential_to_claims =
      [](const Value::Dict& credential) -> std::vector<std::string> {
    const Value::List* claims_list = credential.FindList("claims");
    if (!claims_list) {
      return {};
    }
    std::vector<std::string> claims;
    for (const Value& claim : *claims_list) {
      const Value::Dict* claim_dict = claim.GetIfDict();
      if (!claim_dict) {
        return {};
      }
      const Value::List* paths = claim_dict->FindList("path");
      if (!paths) {
        return {};
      }
      const std::string* claim_name = paths->back().GetIfString();
      if (!claim_name) {
        return {};
      }
      claims.push_back(*claim_name);
    }
    return claims;
  };

  auto meta_to_vct_values =
      [](const Value::Dict& meta) -> std::vector<std::string> {
    const Value::List* vct_values_list = meta.FindList("vct_values");
    if (!vct_values_list) {
      return {};
    }
    std::vector<std::string> vct_values;
    for (const Value& vct_value : *vct_values_list) {
      if (!vct_value.is_string()) {
        return {};
      }
      vct_values.push_back(vct_value.GetString());
    }
    return vct_values;
  };

  const Value::List* credentials = query_dict->FindList("credentials");
  if (!credentials) {
    return false;
  }

  base::flat_set<std::string> all_claims;
  base::flat_set<std::string> all_vct_values;
  for (const Value& credential : *credentials) {
    const Value::Dict* credential_dict = credential.GetIfDict();
    if (!credential_dict) {
      return false;
    }
    std::vector<std::string> credential_claims =
        credential_to_claims(*credential_dict);
    all_claims.insert(credential_claims.begin(), credential_claims.end());

    const Value::Dict* meta_dict = credential_dict->FindDict("meta");
    if (!meta_dict) {
      continue;
    }
    std::vector<std::string> meta_vct_values = meta_to_vct_values(*meta_dict);
    all_vct_values.insert(meta_vct_values.begin(), meta_vct_values.end());
  }
  return std::ranges::all_of(all_claims, CanClaimBypassInterstitial) &&
         std::ranges::all_of(all_vct_values, CanVctValueBypassInterstitial);
}

bool CanRequestCredentialBypassInterstitialForOpenid4vpProtocol(
    const Value& request) {
  CHECK(request.is_dict());
  const Value::Dict* request_dict = &request.GetDict();

  // The request may be a JWT. In that case, we need to parse the JWT to get to
  // the actual request payload.
  std::optional<Value> payload;
  if (const std::string* jwt_str = request_dict->FindString("request")) {
    std::optional<base::Value::List> parsed_jwt = sdjwt::Jwt::Parse(*jwt_str);
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

  if (request_dict->contains("presentation_definition")) {
    return CanRequestCredentialBypassInterstitialForOpenid4vpProtocolWithPresentationDefition(
        *request_dict);
  }

  if (request_dict->contains("dcql_query")) {
    return CanRequestCredentialBypassInterstitialForOpenid4vpProtocolWithDCQL(
        *request_dict);
  }
  return false;
}

bool CanRequestCredentialBypassInterstitialForPreviewProtocol(
    const Value& request) {
  CHECK(request.is_dict());
  const Value::Dict& request_dict = request.GetDict();
  const Value::Dict* selector_dict = request_dict.FindDict("selector");
  if (!selector_dict) {
    return false;
  }

  const std::string* doctype = selector_dict->FindString("doctype");
  if (!doctype || *doctype != kMdlDocumentType) {
    return false;
  }

  const Value::List* fields_list = selector_dict->FindList("fields");
  if (!fields_list || fields_list->size() != 1u) {
    return false;
  }

  const Value::Dict* field_dict = fields_list->front().GetIfDict();
  if (!field_dict) {
    return false;
  }
  const std::string* mdoc_data_element = field_dict->FindString("name");
  return mdoc_data_element && CanClaimBypassInterstitial(*mdoc_data_element);
}

// Returns whether an interstitial should be shown based on the assertions being
// requested.
bool CanRequestCredentialBypassInterstitial(const std::string& protocol,
                                            const Value& request) {
  if (!request.is_dict()) {
    return false;
  }

  if (protocol.starts_with(kOpenid4vpProtocolPrefix)) {
    return CanRequestCredentialBypassInterstitialForOpenid4vpProtocol(request);
  }
  return protocol == kPreviewProtocol &&
         CanRequestCredentialBypassInterstitialForPreviewProtocol(request);
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
    case RequestStatusForMetrics::kErrorNoCredential:
    case RequestStatusForMetrics::kErrorUserDeclined:
    case RequestStatusForMetrics::kErrorOther:
      return blink::mojom::RequestDigitalIdentityStatus::kError;
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
    std::optional<std::string> protocol,
    base::expected<DigitalIdentityProvider::DigitalCredential,
                   RequestStatusForMetrics> response) {
  RequestDigitalIdentityStatus status =
      response.has_value() ? RequestDigitalIdentityStatus::kSuccess
                           : ToRequestDigitalIdentityStatus(response.error());
  CompleteRequestWithStatus(std::move(protocol), status, std::move(response));
}

void DigitalIdentityRequestImpl::CompleteRequestWithError(
    RequestStatusForMetrics status_for_metrics) {
  CompleteRequest(/*protocol=*/std::nullopt,
                  base::unexpected(status_for_metrics));
}

void DigitalIdentityRequestImpl::CompleteRequestWithStatus(
    std::optional<std::string> protocol,
    RequestDigitalIdentityStatus status,
    base::expected<DigitalIdentityProvider::DigitalCredential,
                   RequestStatusForMetrics> response) {
  // Invalidate pending requests in case that the request gets aborted.
  weak_ptr_factory_.InvalidateWeakPtrs();

  provider_.reset();
  update_interstitial_on_abort_callback_.Reset();

  base::UmaHistogramEnumeration("Blink.DigitalIdentityRequest.Status",
                                response.has_value()
                                    ? RequestStatusForMetrics::kSuccess
                                    : response.error());

  if (response.has_value()) {
    // `protocol` is nullopt if and only if there are multiple requests, in
    // which case, the browser cannot pick the protocol and hence rely solely on
    // the protocol in the response from the digital wallet. If absent, an error
    // is returned.
    if (!protocol.has_value() && !response->protocol.has_value()) {
      CompleteRequestWithError(RequestStatusForMetrics::kErrorOther);
      return;
    }
    // The protocol provided in the digital wallet response is preferred. If
    // absent, the protocol specified in the original request will be used
    // instead. This fallback mechanism maintains backward compatibility with
    // digital wallets that do not include the protocol in their response.
    std::move(callback_).Run(
        status, response->protocol.has_value() ? response->protocol : protocol,
        std::move(response->data));
  } else {
    std::move(callback_).Run(status, std::nullopt, std::nullopt);
  }
}

Value BuildGetRequest(
    const std::vector<blink::mojom::DigitalCredentialGetRequestPtr>&
        digital_credential_requests) {
  auto requests = Value::List();
  for (const auto& request : digital_credential_requests) {
    auto result = Value::Dict();
    result.Set("protocol", request->protocol);
    result.Set("data", request->data.Clone());
    requests.Append(std::move(result));
  }
  Value::Dict out = Value::Dict().Set("requests", std::move(requests));
  return Value(std::move(out));
}

Value BuildCreateRequest(
    std::vector<blink::mojom::DigitalCredentialCreateRequestPtr>
        digital_credential_requests) {
  auto requests = Value::List();
  for (const auto& request : digital_credential_requests) {
    auto result = Value::Dict();
    result.Set("protocol", request->protocol);
    result.Set("data", std::move(request->data));
    requests.Append(std::move(result));
  }
  Value::Dict out = Value::Dict().Set("requests", std::move(requests));
  return Value(std::move(out));
}

void DigitalIdentityRequestImpl::Get(
    std::vector<blink::mojom::DigitalCredentialGetRequestPtr>
        digital_credential_requests,
    GetCallback callback) {
  if (!webid::IsDigitalCredentialsEnabled()) {
    std::move(callback).Run(RequestDigitalIdentityStatus::kError,
                            /*protocol=*/std::nullopt, /*token=*/std::nullopt);
    return;
  }

  if (render_frame_host().IsNestedWithinFencedFrame()) {
    mojo::ReportBadMessage(
        "DigitalIdentityRequest should not be allowed in fenced frame "
        "trees.");
    return;
  }

  if (callback_) {
    // Only allow one in-flight wallet request.
    std::move(callback).Run(RequestDigitalIdentityStatus::kErrorTooManyRequests,
                            /*protocol=*/std::nullopt, /*token=*/std::nullopt);
    return;
  }

  callback_ = std::move(callback);

  if (!render_frame_host().HasTransientUserActivation()) {
    CompleteRequestWithError(
        RequestStatusForMetrics::kErrorNoTransientUserActivation);
    return;
  }

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

  // If there is only one request, the protocol can determined without waiting
  // for the wallet response. This is added for backward compatibility with
  // wallet that didn't return the protocol as part of the response.
  std::optional<std::string> protocol =
      digital_credential_requests.size() == 1u
          ? std::make_optional(digital_credential_requests[0]->protocol)
          : std::nullopt;

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeUIForDigitalIdentity)) {
    // Post delayed task to enable testing abort.
    GetUIThreadTaskRunner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DigitalIdentityRequestImpl::CompleteRequest,
                       weak_ptr_factory_.GetWeakPtr(), protocol,
                       DigitalIdentityProvider::DigitalCredential(
                           protocol, Value(Value::Dict().Set(
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

  Value request_to_send = BuildGetRequest(digital_credential_requests);
  if (!interstitial_type) {
    OnInterstitialDone(std::move(protocol), std::move(request_to_send),
                       RequestStatusForMetrics::kSuccess);
    return;
  }

  update_interstitial_on_abort_callback_ =
      provider_->ShowDigitalIdentityInterstitial(
          *WebContents::FromRenderFrameHost(&render_frame_host()), origin(),
          *interstitial_type,
          base::BindOnce(&DigitalIdentityRequestImpl::OnInterstitialDone,
                         weak_ptr_factory_.GetWeakPtr(), std::move(protocol),
                         std::move(request_to_send)));
}

void DigitalIdentityRequestImpl::Create(
    std::vector<blink::mojom::DigitalCredentialCreateRequestPtr>
        digital_credential_requests,
    CreateCallback callback) {
  if (!webid::IsDigitalCredentialsCreationEnabled()) {
    std::move(callback).Run(RequestDigitalIdentityStatus::kError,
                            /*protocol=*/std::nullopt, /*token=*/std::nullopt);
    return;
  }

  if (render_frame_host().IsNestedWithinFencedFrame()) {
    mojo::ReportBadMessage(
        "DigitalIdentityRequest should not be allowed in fenced frame "
        "trees.");
    return;
  }

  if (callback_) {
    // Only allow one in-flight wallet request.
    std::move(callback).Run(RequestDigitalIdentityStatus::kErrorTooManyRequests,
                            /*protocol=*/std::nullopt, /*token=*/std::nullopt);
    return;
  }

  callback_ = std::move(callback);

  if (!render_frame_host().HasTransientUserActivation()) {
    CompleteRequestWithError(
        RequestStatusForMetrics::kErrorNoTransientUserActivation);
    return;
  }

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

  Value request_to_send =
      BuildCreateRequest(std::move(digital_credential_requests));

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeUIForDigitalIdentity)) {
    // Post delayed task to enable testing abort+.
    GetUIThreadTaskRunner()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DigitalIdentityRequestImpl::CompleteRequest,
                       weak_ptr_factory_.GetWeakPtr(), protocol,
                       DigitalIdentityProvider::DigitalCredential(
                           protocol, Value(Value::Dict().Set(
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
  // TODO(crbug.com/378330032): Instead of passing the protocol here, it should
  // be read from the wallet response.
  provider_->Create(WebContents::FromRenderFrameHost(&render_frame_host()),
                    origin(), request_to_send,
                    base::BindOnce(&DigitalIdentityRequestImpl::CompleteRequest,
                                   weak_ptr_factory_.GetWeakPtr(), protocol));
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
      /*protocol=*/std::nullopt, RequestDigitalIdentityStatus::kErrorCanceled,
      base::unexpected(RequestStatusForMetrics::kErrorAborted));
}

void DigitalIdentityRequestImpl::OnInterstitialDone(
    std::optional<std::string> protocol,
    Value request_to_send,
    RequestStatusForMetrics status_after_interstitial) {
  if (status_after_interstitial != RequestStatusForMetrics::kSuccess) {
    CompleteRequestWithError(status_after_interstitial);
    return;
  }

  provider_->Get(
      WebContents::FromRenderFrameHost(&render_frame_host()), origin(),
      request_to_send,
      base::BindOnce(&DigitalIdentityRequestImpl::CompleteRequest,
                     weak_ptr_factory_.GetWeakPtr(), std::move(protocol)));
}

}  // namespace content
