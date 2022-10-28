// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/webauth_request_security_checker.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "content/browser/bad_message.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "device/fido/features.h"
#include "device/fido/fido_transport_protocol.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/permissions_policy/permissions_policy_feature.mojom.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace content {

namespace {

// Returns AuthenticatorStatus::SUCCESS if the caller origin is in principle
// authorized to make WebAuthn requests, and an error if it fails one of the
// criteria below.
//
// Reference https://url.spec.whatwg.org/#valid-domain-string and
// https://html.spec.whatwg.org/multipage/origin.html#concept-origin-effective-domain.
blink::mojom::AuthenticatorStatus OriginAllowedToMakeWebAuthnRequests(
    url::Origin caller_origin) {
  if (caller_origin.opaque()) {
    return blink::mojom::AuthenticatorStatus::OPAQUE_DOMAIN;
  }

  // The scheme is required to be HTTP(S).  Given the
  // |network::IsUrlPotentiallyTrustworthy| check below, HTTP is effectively
  // restricted to just "localhost".
  if (caller_origin.scheme() != url::kHttpScheme &&
      caller_origin.scheme() != url::kHttpsScheme) {
    return blink::mojom::AuthenticatorStatus::INVALID_PROTOCOL;
  }

  // TODO(https://crbug.com/1158302): Use IsOriginPotentiallyTrustworthy?
  if (url::HostIsIPAddress(caller_origin.host()) ||
      !network::IsUrlPotentiallyTrustworthy(caller_origin.GetURL())) {
    return blink::mojom::AuthenticatorStatus::INVALID_DOMAIN;
  }

  return blink::mojom::AuthenticatorStatus::SUCCESS;
}

// Returns whether a caller origin is allowed to claim a given Relying Party ID.
// It's valid for the requested RP ID to be a registrable domain suffix of, or
// be equal to, the origin's effective domain.  Reference:
// https://html.spec.whatwg.org/multipage/origin.html#is-a-registrable-domain-suffix-of-or-is-equal-to.
bool OriginIsAllowedToClaimRelyingPartyId(
    const std::string& claimed_relying_party_id,
    const url::Origin& caller_origin) {
  // `OriginAllowedToMakeWebAuthnRequests()` must have been called before.
  DCHECK_EQ(OriginAllowedToMakeWebAuthnRequests(caller_origin),
            blink::mojom::AuthenticatorStatus::SUCCESS);

  if (claimed_relying_party_id.empty()) {
    return false;
  }

  if (caller_origin.host() == claimed_relying_party_id) {
    return true;
  }

  if (!caller_origin.DomainIs(claimed_relying_party_id)) {
    return false;
  }

  if (!net::registry_controlled_domains::HostHasRegistryControlledDomain(
          caller_origin.host(),
          net::registry_controlled_domains::INCLUDE_UNKNOWN_REGISTRIES,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES) ||
      !net::registry_controlled_domains::HostHasRegistryControlledDomain(
          claimed_relying_party_id,
          net::registry_controlled_domains::INCLUDE_UNKNOWN_REGISTRIES,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    // TODO(crbug.com/803414): Accept corner-case situations like the
    // following origin: "https://login.awesomecompany", relying_party_id:
    // "awesomecompany".
    return false;
  }

  return true;
}

}  // namespace

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

  // MakeCredential requests do not have an associated permissions policy, but
  // are prohibited in cross-origin subframes.
  if (!*is_cross_origin && type == RequestType::kMakeCredential) {
    return blink::mojom::AuthenticatorStatus::SUCCESS;
  }

  // Requests in cross-origin iframes are permitted if enabled via permissions
  // policy and for SPC requests.
  if (type == RequestType::kGetAssertion &&
      render_frame_host_->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kPublicKeyCredentialsGet)) {
    return blink::mojom::AuthenticatorStatus::SUCCESS;
  }
  if ((type == RequestType::kMakePaymentCredential ||
       type == RequestType::kGetPaymentCredentialAssertion) &&
      render_frame_host_->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kPayment)) {
    return blink::mojom::AuthenticatorStatus::SUCCESS;
  }
  return blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR;
}

blink::mojom::AuthenticatorStatus
WebAuthRequestSecurityChecker::ValidateDomainAndRelyingPartyID(
    const url::Origin& caller_origin,
    const std::string& relying_party_id,
    RequestType request_type,
    const blink::mojom::RemoteDesktopClientOverridePtr&
        remote_desktop_client_override) {
  if (GetContentClient()
          ->browser()
          ->GetWebAuthenticationDelegate()
          ->OverrideCallerOriginAndRelyingPartyIdValidation(
              render_frame_host_->GetBrowserContext(), caller_origin,
              relying_party_id)) {
    return blink::mojom::AuthenticatorStatus::SUCCESS;
  }

  blink::mojom::AuthenticatorStatus domain_validation =
      OriginAllowedToMakeWebAuthnRequests(caller_origin);
  if (domain_validation != blink::mojom::AuthenticatorStatus::SUCCESS) {
    return domain_validation;
  }

  // SecurePaymentConfirmation allows third party payment service provider to
  // get assertions on behalf of the Relying Parties. Hence it is not required
  // for the RP ID to be a registrable suffix of the caller origin, as it would
  // be for WebAuthn requests.
  if (request_type == RequestType::kGetPaymentCredentialAssertion)
    return blink::mojom::AuthenticatorStatus::SUCCESS;

  url::Origin relying_party_origin = caller_origin;
  if (remote_desktop_client_override) {
    if (!GetContentClient()
             ->browser()
             ->GetWebAuthenticationDelegate()
             ->OriginMayUseRemoteDesktopClientOverride(
                 render_frame_host_->GetBrowserContext(), caller_origin)) {
      return blink::mojom::AuthenticatorStatus::
          REMOTE_DESKTOP_CLIENT_OVERRIDE_NOT_AUTHORIZED;
    }
    relying_party_origin = remote_desktop_client_override->origin;
  }

  if (!OriginIsAllowedToClaimRelyingPartyId(relying_party_id,
                                            relying_party_origin)) {
    return blink::mojom::AuthenticatorStatus::BAD_RELYING_PARTY_ID;
  }
  return blink::mojom::AuthenticatorStatus::SUCCESS;
}

blink::mojom::AuthenticatorStatus
WebAuthRequestSecurityChecker::ValidateAppIdExtension(
    std::string appid,
    url::Origin caller_origin,
    const blink::mojom::RemoteDesktopClientOverridePtr&
        remote_desktop_client_override,
    std::string* out_appid) {
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
