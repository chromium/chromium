// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/webauth_request_security_checker.h"

#include <string_view>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "content/browser/bad_message.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/webauthn_security_utils.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "device/fido/features.h"
#include "device/fido/fido_transport_protocol.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/base/url_util.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

#if !BUILDFLAG(IS_ANDROID)
#include "content/public/browser/authenticator_request_client_delegate.h"
#endif

namespace content {

static const net::NetworkTrafficAnnotationTag kRpIdCheckTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("webauthn_rp_id_check", R"(
        semantics {
          sender: "Web Authentication"
          description:
            "WebAuthn credentials are bound to domain names. If a web site "
            "attempts to use a credential owned by a different domain then a "
            "network request is made to the owning domain to see whether the "
            "calling origin is authorized."
          trigger:
            "A web-site initiates a WebAuthn request and the requested RP ID "
            "cannot be trivially validated."
          user_data {
            type: WEB_CONTENT
          }
          data: "None sent. Response is public information from the target "
                "domain, or an error."
          internal {
            contacts {
              email: "chrome-webauthn@google.com"
            }
          }
          destination: WEBSITE
          last_reviewed: "2023-10-31"
        }
        policy {
          cookies_allowed: NO
          setting: "Not controlled by a setting because the operation is "
            "triggered by web sites and is needed to implement the "
            "WebAuthn API."
          policy_exception_justification:
            "No policy provided because the operation is triggered by "
            "websites to fetch public information. No background activity "
            "occurs."
        })");

// kRpIdMaxBodyBytes is the maximum number of bytes that we'll download in order
// to validate an RP ID.
constexpr size_t kRpIdMaxBodyBytes = 1u << 18;

WebAuthRequestSecurityChecker::RemoteValidation::~RemoteValidation() = default;

// static
std::unique_ptr<WebAuthRequestSecurityChecker::RemoteValidation>
WebAuthRequestSecurityChecker::RemoteValidation::Create(
    const url::Origin& caller_origin,
    const std::string& relying_party_id,
    base::OnceCallback<void(blink::mojom::AuthenticatorStatus)> callback) {
  // The relying party may allow other origins to use its RP ID based on the
  // contents of a .well-known file.
  std::string canonicalized_domain_storage;
  url::StdStringCanonOutput canon_output(&canonicalized_domain_storage);
  url::CanonHostInfo host_info;
  url::CanonicalizeHostVerbose(relying_party_id.data(),
                               url::Component(0, relying_party_id.size()),
                               &canon_output, &host_info);
  const std::string_view canonicalized_domain(canon_output.data(),
                                              canon_output.length());
  if (host_info.family != url::CanonHostInfo::Family::NEUTRAL ||
      !net::IsCanonicalizedHostCompliant(canonicalized_domain)) {
    // The RP ID must look like a hostname, e.g. not an IP address.
    std::move(callback).Run(
        blink::mojom::AuthenticatorStatus::BAD_RELYING_PARTY_ID);
    return nullptr;
  }

  constexpr char well_known_url_template[] =
      "https://domain.com/.well-known/webauthn";
  GURL well_known_url(well_known_url_template);
  CHECK(well_known_url.is_valid());

  GURL::Replacements replace_host;
  replace_host.SetHostStr(canonicalized_domain);
  well_known_url = well_known_url.ReplaceComponents(replace_host);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      GetContentClient()->browser()->GetSystemSharedURLLoaderFactory();
  if (!url_loader_factory) {
    std::move(callback).Run(
        blink::mojom::AuthenticatorStatus::BAD_RELYING_PARTY_ID);
    return nullptr;
  }

  auto network_request = std::make_unique<network::ResourceRequest>();
  network_request->url = well_known_url;

  std::unique_ptr<RemoteValidation> validation(
      new RemoteValidation(caller_origin, std::move(callback)));

  validation->loader_ = network::SimpleURLLoader::Create(
      std::move(network_request), kRpIdCheckTrafficAnnotation);
  validation->loader_->SetTimeoutDuration(base::Seconds(10));
  validation->loader_->SetURLLoaderFactoryOptions(
      network::mojom::kURLLoadOptionBlockAllCookies);
  validation->loader_->DownloadToString(
      url_loader_factory.get(),
      base::BindOnce(&RemoteValidation::OnFetchComplete,
                     // `validation` owns the `SimpleURLLoader` so if it's
                     // deleted, the loader will be too.
                     base::Unretained(validation.get())),
      kRpIdMaxBodyBytes);

  return validation;
}

// static
blink::mojom::AuthenticatorStatus
WebAuthRequestSecurityChecker::RemoteValidation::ValidateWellKnownJSON(
    const url::Origin& caller_origin,
    const base::Value& value) {
  // This code processes a .well-known/webauthn JSON. See
  // https://github.com/w3c/webauthn/wiki/Explainer:-Related-origin-requests

  if (!value.is_dict()) {
    return blink::mojom::AuthenticatorStatus::
        BAD_RELYING_PARTY_ID_JSON_PARSE_ERROR;
  }

  const base::Value::List* origins = value.GetDict().FindList("origins");
  if (!origins) {
    return blink::mojom::AuthenticatorStatus::
        BAD_RELYING_PARTY_ID_JSON_PARSE_ERROR;
  }

  constexpr size_t kMaxLabels = 5;
  bool hit_limits = false;
  base::flat_set<std::string> labels_seen;
  for (const base::Value& origin_str : *origins) {
    if (!origin_str.is_string()) {
      return blink::mojom::AuthenticatorStatus::
          BAD_RELYING_PARTY_ID_JSON_PARSE_ERROR;
    }

    const GURL url(origin_str.GetString());
    if (!url.is_valid()) {
      continue;
    }

    const std::string domain =
        net::registry_controlled_domains::GetDomainAndRegistry(
            url, net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES);
    if (domain.empty()) {
      continue;
    }

    const std::string::size_type dot_index = domain.find('.');
    if (dot_index == std::string::npos) {
      continue;
    }

    const std::string etld_plus_1_label = domain.substr(0, dot_index);
    if (!base::Contains(labels_seen, etld_plus_1_label)) {
      if (labels_seen.size() >= kMaxLabels) {
        hit_limits = true;
        continue;
      }
      labels_seen.insert(etld_plus_1_label);
    }

    const auto origin = url::Origin::Create(url);
    if (origin.IsSameOriginWith(caller_origin)) {
      return blink::mojom::AuthenticatorStatus::SUCCESS;
    }
  }

  if (hit_limits) {
    return blink::mojom::AuthenticatorStatus::
        BAD_RELYING_PARTY_ID_NO_JSON_MATCH_HIT_LIMITS;
  }
  return blink::mojom::AuthenticatorStatus::BAD_RELYING_PARTY_ID_NO_JSON_MATCH;
}

WebAuthRequestSecurityChecker::RemoteValidation::RemoteValidation(
    const url::Origin& caller_origin,
    base::OnceCallback<void(blink::mojom::AuthenticatorStatus)> callback)
    : caller_origin_(caller_origin), callback_(std::move(callback)) {}

// OnFetchComplete is called when the `.well-known/webauthn` for an
// RP ID has finished downloading.
void WebAuthRequestSecurityChecker::RemoteValidation::OnFetchComplete(
    std::unique_ptr<std::string> body) {
  if (!body) {
    std::move(callback_).Run(blink::mojom::AuthenticatorStatus::
                                 BAD_RELYING_PARTY_ID_ATTEMPTED_FETCH);
    return;
  }

  if (loader_->ResponseInfo()->mime_type != "application/json") {
    std::move(callback_).Run(blink::mojom::AuthenticatorStatus::
                                 BAD_RELYING_PARTY_ID_WRONG_CONTENT_TYPE);
    return;
  }

  json_ = std::move(body);
  data_decoder::DataDecoder::ParseJsonIsolated(
      *json_, base::BindOnce(&RemoteValidation::OnDecodeComplete,
                             weak_factory_.GetWeakPtr()));
}

void WebAuthRequestSecurityChecker::RemoteValidation::OnDecodeComplete(
    base::expected<base::Value, std::string> maybe_value) {
  blink::mojom::AuthenticatorStatus status =
      blink::mojom::AuthenticatorStatus::BAD_RELYING_PARTY_ID_JSON_PARSE_ERROR;
  if (maybe_value.has_value()) {
    status = ValidateWellKnownJSON(caller_origin_, maybe_value.value());
  }
  std::move(callback_).Run(status);
}

WebAuthRequestSecurityChecker::WebAuthRequestSecurityChecker(
    RenderFrameHost* host)
    : render_frame_host_(host) {}

WebAuthRequestSecurityChecker::~WebAuthRequestSecurityChecker() = default;

bool WebAuthRequestSecurityChecker::IsSameOriginWithAncestors(
    const url::Origin& origin) {
  RenderFrameHost* parent = render_frame_host_->GetParentOrOuterDocument();
  while (parent) {
    if (!parent->GetLastCommittedOrigin().IsSameOriginWith(origin))
      return false;
    parent = parent->GetParentOrOuterDocument();
  }
  return true;
}

blink::mojom::AuthenticatorStatus
WebAuthRequestSecurityChecker::ValidateAncestorOrigins(
    const url::Origin& origin,
    RequestType type,
    bool* is_cross_origin) {
  if (render_frame_host_->IsNestedWithinFencedFrame()) {
    bad_message::ReceivedBadMessage(
        render_frame_host_->GetProcess(),
        bad_message::BadMessageReason::AUTH_INVALID_FENCED_FRAME);
    return blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR;
  }

  *is_cross_origin = !IsSameOriginWithAncestors(origin);

  // Requests in cross-origin iframes are permitted if enabled via permissions
  // policy and for SPC requests.
  if (type == RequestType::kMakeCredential &&
      render_frame_host_->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::
              kPublicKeyCredentialsCreate)) {
    return blink::mojom::AuthenticatorStatus::SUCCESS;
  }
  if (type == RequestType::kGetAssertion &&
      render_frame_host_->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kPublicKeyCredentialsGet)) {
    return blink::mojom::AuthenticatorStatus::SUCCESS;
  }
  // For credential creation, SPC credentials (i.e., credentials with the
  // "payment" extension) may use either the 'publickey-credentials-create' or
  // 'payment' permissions policy.
  if (type == RequestType::kMakePaymentCredential) {
    if (render_frame_host_->IsFeatureEnabled(
            blink::mojom::PermissionsPolicyFeature::
                kPublicKeyCredentialsCreate) ||
        render_frame_host_->IsFeatureEnabled(
            blink::mojom::PermissionsPolicyFeature::kPayment)) {
      return blink::mojom::AuthenticatorStatus::SUCCESS;
    }
  }

  if (type == RequestType::kGetPaymentCredentialAssertion &&
      render_frame_host_->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kPayment)) {
    return blink::mojom::AuthenticatorStatus::SUCCESS;
  }
  // TODO(crbug.com/347727501): Add a permissions policy for report.
  if (type == RequestType::kReport) {
    return blink::mojom::AuthenticatorStatus::SUCCESS;
  }

  return blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR;
}

std::unique_ptr<WebAuthRequestSecurityChecker::RemoteValidation>
WebAuthRequestSecurityChecker::ValidateDomainAndRelyingPartyID(
    const url::Origin& caller_origin,
    const std::string& relying_party_id,
    RequestType request_type,
    const blink::mojom::RemoteDesktopClientOverridePtr&
        remote_desktop_client_override,
    base::OnceCallback<void(blink::mojom::AuthenticatorStatus)> callback) {
#if !BUILDFLAG(IS_ANDROID)
  // Extensions are not supported on Android.
  if (GetContentClient()
          ->browser()
          ->GetWebAuthenticationDelegate()
          ->OverrideCallerOriginAndRelyingPartyIdValidation(
              render_frame_host_->GetBrowserContext(), caller_origin,
              relying_party_id)) {
    std::move(callback).Run(blink::mojom::AuthenticatorStatus::SUCCESS);
    return nullptr;
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  blink::mojom::AuthenticatorStatus domain_validation =
      OriginAllowedToMakeWebAuthnRequests(caller_origin);
  if (domain_validation != blink::mojom::AuthenticatorStatus::SUCCESS) {
    std::move(callback).Run(domain_validation);
    return nullptr;
  }

  // SecurePaymentConfirmation allows third party payment service provider to
  // get assertions on behalf of the Relying Parties. Hence it is not required
  // for the RP ID to be a registrable suffix of the caller origin, as it would
  // be for WebAuthn requests.
  if (request_type == RequestType::kGetPaymentCredentialAssertion) {
    std::move(callback).Run(blink::mojom::AuthenticatorStatus::SUCCESS);
    return nullptr;
  }

  url::Origin relying_party_origin = caller_origin;
#if !BUILDFLAG(IS_ANDROID)
  if (remote_desktop_client_override) {
    if (!GetContentClient()
             ->browser()
             ->GetWebAuthenticationDelegate()
             ->OriginMayUseRemoteDesktopClientOverride(
                 render_frame_host_->GetBrowserContext(), caller_origin)) {
      std::move(callback).Run(
          blink::mojom::AuthenticatorStatus::
              REMOTE_DESKTOP_CLIENT_OVERRIDE_NOT_AUTHORIZED);
      return nullptr;
    }
    relying_party_origin = remote_desktop_client_override->origin;
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  if (OriginIsAllowedToClaimRelyingPartyId(relying_party_id,
                                           relying_party_origin)) {
    std::move(callback).Run(blink::mojom::AuthenticatorStatus::SUCCESS);
    return nullptr;
  }

  if (base::FeatureList::IsEnabled(device::kWebAuthnRelatedOrigin)) {
    return RemoteValidation::Create(caller_origin, relying_party_id,
                                    std::move(callback));
  }

  std::move(callback).Run(
      blink::mojom::AuthenticatorStatus::BAD_RELYING_PARTY_ID);
  return nullptr;
}

blink::mojom::AuthenticatorStatus
WebAuthRequestSecurityChecker::ValidateAppIdExtension(
    std::string appid,
    url::Origin caller_origin,
    const blink::mojom::RemoteDesktopClientOverridePtr&
        remote_desktop_client_override,
    std::string* out_appid) {
#if !BUILDFLAG(IS_ANDROID)
  if (remote_desktop_client_override) {
    if (!GetContentClient()
             ->browser()
             ->GetWebAuthenticationDelegate()
             ->OriginMayUseRemoteDesktopClientOverride(
                 render_frame_host_->GetBrowserContext(), caller_origin)) {
      return blink::mojom::AuthenticatorStatus::
          REMOTE_DESKTOP_CLIENT_OVERRIDE_NOT_AUTHORIZED;
    }
    caller_origin = remote_desktop_client_override->origin;
  }
#endif  // !BUILDFLAG(IS_ANDROID)

  // Step 1: "If the AppID is not an HTTPS URL, and matches the FacetID of the
  // caller, no additional processing is necessary and the operation may
  // proceed."

  // Webauthn is only supported on secure origins and
  // `ValidateDomainAndRelyingPartyID()` has already checked this property of
  // `caller_origin` before this call. Thus this step is moot.
  DCHECK(network::IsOriginPotentiallyTrustworthy(caller_origin));

  // Step 2: "If the AppID is null or empty, the client must set the AppID to be
  // the FacetID of the caller, and the operation may proceed without additional
  // processing."
  if (appid.empty()) {
    // While the U2F spec says to default the App ID to the Facet ID, which is
    // the origin plus a trailing forward slash [1], implementations of U2F
    // (CryptoToken, Firefox) used to use the site's Origin without trailing
    // slash. We follow their implementations rather than the spec.
    //
    // [1]https://fidoalliance.org/specs/fido-v2.0-id-20180227/fido-appid-and-facets-v2.0-id-20180227.html#determining-the-facetid-of-a-calling-application
    appid = caller_origin.Serialize();
  }

  // Step 3: "If the caller's FacetID is an https:// Origin sharing the same
  // host as the AppID, (e.g. if an application hosted at
  // https://fido.example.com/myApp set an AppID of
  // https://fido.example.com/myAppId), no additional processing is necessary
  // and the operation may proceed."
  GURL appid_url = GURL(appid);
  if (!appid_url.is_valid() || appid_url.scheme() != url::kHttpsScheme ||
      appid_url.scheme_piece() != caller_origin.scheme()) {
    return blink::mojom::AuthenticatorStatus::INVALID_DOMAIN;
  }

  // This check is repeated inside |SameDomainOrHost|, just after this. However
  // it's cheap and mirrors the structure of the spec.
  if (appid_url.host_piece() == caller_origin.host()) {
    *out_appid = appid;
    return blink::mojom::AuthenticatorStatus::SUCCESS;
  }

  // At this point we diverge from the specification in order to avoid the
  // complexity of making a network request which isn't believed to be
  // necessary in practice. See also
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1244959#c8
  if (net::registry_controlled_domains::SameDomainOrHost(
          appid_url, caller_origin,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    *out_appid = appid;
    return blink::mojom::AuthenticatorStatus::SUCCESS;
  }

  // As a compatibility hack, sites within google.com are allowed to assert two
  // special-case AppIDs. Firefox also does this:
  // https://groups.google.com/forum/#!msg/mozilla.dev.platform/Uiu3fwnA2xw/201ynAiPAQAJ
  const GURL gstatic_appid(kGstaticAppId);
  const GURL gstatic_corp_appid(kGstaticCorpAppId);
  DCHECK(gstatic_appid.is_valid() && gstatic_corp_appid.is_valid());
  if (caller_origin.DomainIs("google.com") && !appid_url.has_ref() &&
      (appid_url.EqualsIgnoringRef(gstatic_appid) ||
       appid_url.EqualsIgnoringRef(gstatic_corp_appid))) {
    *out_appid = appid;
    return blink::mojom::AuthenticatorStatus::SUCCESS;
  }

  return blink::mojom::AuthenticatorStatus::INVALID_DOMAIN;
}

bool WebAuthRequestSecurityChecker::
    DeduplicateCredentialDescriptorListAndValidateLength(
        std::vector<device::PublicKeyCredentialDescriptor>* list) {
  // Credential descriptor lists should not exceed 64 entries, which is enforced
  // by renderer code. Any duplicate entries they contain should be ignored.
  // This is to guard against sites trying to amplify small timing differences
  // in the processing of different types of credentials when sending probing
  // requests to physical security keys (https://crbug.com/1248862).
  if (list->size() > blink::mojom::kPublicKeyCredentialDescriptorListMaxSize) {
    return false;
  }
  auto credential_descriptor_compare_without_transport =
      [](const device::PublicKeyCredentialDescriptor& a,
         const device::PublicKeyCredentialDescriptor& b) {
        return a.credential_type < b.credential_type ||
               (a.credential_type == b.credential_type && a.id < b.id);
      };
  std::set<device::PublicKeyCredentialDescriptor,
           decltype(credential_descriptor_compare_without_transport)>
      unique_credential_descriptors(
          credential_descriptor_compare_without_transport);
  for (const auto& credential_descriptor : *list) {
    auto it = unique_credential_descriptors.find(credential_descriptor);
    if (it == unique_credential_descriptors.end()) {
      unique_credential_descriptors.insert(credential_descriptor);
    } else {
      // Combine transport hints of descriptors with identical IDs. Empty
      // transport list means _any_ transport, so the union should still be
      // empty.
      base::flat_set<device::FidoTransportProtocol> merged_transports;
      if (!it->transports.empty() &&
          !credential_descriptor.transports.empty()) {
        base::ranges::set_union(
            it->transports, credential_descriptor.transports,
            std::inserter(merged_transports, merged_transports.begin()));
      }
      unique_credential_descriptors.erase(it);
      unique_credential_descriptors.insert(
          {credential_descriptor.credential_type, credential_descriptor.id,
           std::move(merged_transports)});
    }
  }
  *list = {unique_credential_descriptors.begin(),
           unique_credential_descriptors.end()};
  return true;
}

}  // namespace content
