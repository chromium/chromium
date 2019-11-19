// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/authenticator_common.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "base/base64url.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/string_piece.h"
#include "base/timer/timer.h"
#include "content/browser/bad_message.h"
#include "content/browser/webauth/authenticator_environment_impl.h"
#include "content/browser/webauth/virtual_authenticator_request_delegate.h"
#include "content/browser/webauth/virtual_fido_discovery_factory.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/system_connector.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/origin_util.h"
#include "crypto/sha2.h"
#include "device/base/features.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/fido/attestation_statement.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/features.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/get_assertion_request_handler.h"
#include "device/fido/make_credential_request_handler.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_params.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "net/cert/asn1_util.h"
#include "net/der/input.h"
#include "net/der/parse_values.h"
#include "net/der/parser.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/service_manager/public/cpp/connector.h"
#include "url/url_constants.h"
#include "url/url_util.h"

namespace content {

namespace client_data {
const char kCreateType[] = "webauthn.create";
const char kGetType[] = "webauthn.get";
const char kU2fSignType[] = "navigator.id.getAssertion";
const char kU2fRegisterType[] = "navigator.id.finishEnrollment";
}  // namespace client_data

namespace {

constexpr char kCryptotokenOrigin[] =
    "chrome-extension://kmendfapggjehodndflmmgagdbamhnfd";

// AttestationPromptResult enumerates events related to attestation prompts.
// These values are recorded in an UMA histogram and so should not be
// reassigned.
enum class AttestationPromptResult {
  // kQueried indicates that the embedder was queried in order to determine
  // whether attestation information should be returned to the origin.
  kQueried = 0,
  // kTimeout indicates that a timeout occurred while awaiting the result of an
  // attestation query.
  kTimeout = 1,
  // kAllowed indicates that the query to the embedder was resolved positively.
  // (E.g. the user clicked to allow, or the embedded allowed immediately by
  // policy.)
  kAllowed = 2,
  // kBlocked indicates that the query to the embedder was resolved negatively.
  // (E.g. the user clicked to block, or closed the dialog.)
  kBlocked = 3,
  // kAbandoned indications that the user closed the tab or navigated away while
  // the attestation prompt was showing.
  kAbandoned = 4,
  kMaxValue = kAbandoned,
};

// The following enums correspond to UMA histograms and should not be
// reassigned.
enum class RelyingPartySecurityCheckFailure {
  kOpaqueOrNonSecureOrigin = 0,
  kRelyingPartyIdInvalid = 1,
  kAppIdExtensionInvalid = 2,
  kAppIdExtensionDomainMismatch = 3,
  kIconUrlInvalid = 4,
  kMaxValue = kIconUrlInvalid,
};

void ReportSecurityCheckFailure(RelyingPartySecurityCheckFailure error) {
  UMA_HISTOGRAM_ENUMERATION(
      "WebAuthentication.RelyingPartySecurityCheckFailure", error);
}

bool OriginIsCryptoTokenExtension(const url::Origin& origin) {
  auto cryptotoken_origin = url::Origin::Create(GURL(kCryptotokenOrigin));
  return cryptotoken_origin == origin;
}

// Returns AuthenticatorStatus::SUCCESS if the domain is valid and an error
// if it fails one of the criteria below.
// Reference https://url.spec.whatwg.org/#valid-domain-string and
// https://html.spec.whatwg.org/multipage/origin.html#concept-origin-effective-domain.
blink::mojom::AuthenticatorStatus ValidateEffectiveDomain(
    url::Origin caller_origin) {
  // For calls originating in the CryptoToken U2F extension, allow CryptoToken
  // to validate domain.
  if (OriginIsCryptoTokenExtension(caller_origin)) {
    return blink::mojom::AuthenticatorStatus::SUCCESS;
  }

  if (caller_origin.opaque()) {
    return blink::mojom::AuthenticatorStatus::OPAQUE_DOMAIN;
  }

  if (url::HostIsIPAddress(caller_origin.host()) ||
      !content::IsOriginSecure(caller_origin.GetURL())) {
    return blink::mojom::AuthenticatorStatus::INVALID_DOMAIN;
  }

  // Additionally, the scheme is required to be HTTP(S). Other schemes
  // may be supported in the future but the webauthn relying party is
  // just the domain of the origin so we would have to define how the
  // authority part of other schemes maps to a "domain" without
  // collisions. Given the |IsOriginSecure| check, just above, HTTP is
  // effectively restricted to just "localhost".
  if (caller_origin.scheme() != url::kHttpScheme &&
      caller_origin.scheme() != url::kHttpsScheme) {
    return blink::mojom::AuthenticatorStatus::INVALID_PROTOCOL;
  }

  return blink::mojom::AuthenticatorStatus::SUCCESS;
}

// Ensure the relying party ID is a registrable domain suffix of or equal
// to the origin's effective domain. Reference:
// https://html.spec.whatwg.org/multipage/origin.html#is-a-registrable-domain-suffix-of-or-is-equal-to.
bool IsRelyingPartyIdValid(const std::string& relying_party_id,
                           url::Origin caller_origin) {
  if (OriginIsCryptoTokenExtension(caller_origin)) {
    return true;
  }

  if (relying_party_id.empty())
    return false;

  if (caller_origin.host() == relying_party_id)
    return true;

  if (!caller_origin.DomainIs(relying_party_id))
    return false;
  if (!net::registry_controlled_domains::HostHasRegistryControlledDomain(
          caller_origin.host(),
          net::registry_controlled_domains::INCLUDE_UNKNOWN_REGISTRIES,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES))
    return false;
  if (!net::registry_controlled_domains::HostHasRegistryControlledDomain(
          relying_party_id,
          net::registry_controlled_domains::INCLUDE_UNKNOWN_REGISTRIES,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES))
    // TODO(crbug.com/803414): Accept corner-case situations like the following
    // origin: "https://login.awesomecompany",
    // relying_party_id: "awesomecompany".
    return false;
  return true;
}

// Checks if the icon URL is an a-priori authenticated URL.
// https://w3c.github.io/webappsec-credential-management/#dom-credentialuserdata-iconurl
bool IsAPrioriAuthenticatedUrl(const base::Optional<GURL>& url_opt) {
  if (!url_opt)
    return true;

  const auto& url = *url_opt;
  if (url.is_empty())
    return true;

  if (!url.is_valid()) {
    return false;
  }

  // https://www.w3.org/TR/mixed-content/#a-priori-authenticated-url
  return url.IsAboutSrcdoc() || url.IsAboutBlank() ||
         url.SchemeIs(url::kDataScheme) ||
         network::IsUrlPotentiallyTrustworthy(url);
}

// Validates whether the given origin is authorized to use the provided App
// ID value, mostly according to the rules in
// https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-appid-and-facets-v1.2-ps-20170411.html#determining-if-a-caller-s-facetid-is-authorized-for-an-appid.
//
// Returns the App ID to use for the request, or base::nullopt if the origin
// is not authorized to use the provided value.
base::Optional<std::string> ProcessAppIdExtension(std::string appid,
                                                  const url::Origin& origin) {
  // The CryptoToken U2F extension checks the appid before calling the WebAuthn
  // API so there is no need to validate it here.
  if (OriginIsCryptoTokenExtension(origin)) {
    if (!GURL(appid).is_valid()) {
      DCHECK(false) << "cryptotoken request did not set a valid App ID";
      return base::nullopt;
    }
    return appid;
  }

  // Step 1: "If the AppID is not an HTTPS URL, and matches the FacetID of the
  // caller, no additional processing is necessary and the operation may
  // proceed."

  // Webauthn is only supported on secure origins and |ValidateEffectiveDomain|
  // has already checked this property of |origin| before this call. Thus this
  // step is moot.
  DCHECK(content::IsOriginSecure(origin.GetURL()));

  // Step 2: "If the AppID is null or empty, the client must set the AppID to be
  // the FacetID of the caller, and the operation may proceed without additional
  // processing."
  if (appid.empty()) {
    // While the U2F spec says to default the App ID to the Facet ID, which is
    // the origin plus a trailing forward slash [1], cryptotoken and Firefox
    // just use the site's Origin without trailing slash. We follow their
    // implementations rather than the spec.
    //
    // [1]https://fidoalliance.org/specs/fido-v2.0-id-20180227/fido-appid-and-facets-v2.0-id-20180227.html#determining-the-facetid-of-a-calling-application
    appid = origin.Serialize();
  }

  // Step 3: "If the caller's FacetID is an https:// Origin sharing the same
  // host as the AppID, (e.g. if an application hosted at
  // https://fido.example.com/myApp set an AppID of
  // https://fido.example.com/myAppId), no additional processing is necessary
  // and the operation may proceed."
  GURL appid_url = GURL(appid);
  if (!appid_url.is_valid() || appid_url.scheme() != url::kHttpsScheme ||
      appid_url.scheme_piece() != origin.scheme()) {
    ReportSecurityCheckFailure(
        RelyingPartySecurityCheckFailure::kAppIdExtensionInvalid);
    return base::nullopt;
  }

  // This check is repeated inside |SameDomainOrHost|, just after this. However
  // it's cheap and mirrors the structure of the spec.
  if (appid_url.host_piece() == origin.host()) {
    return appid;
  }

  // At this point we diverge from the specification in order to avoid the
  // complexity of making a network request which isn't believed to be
  // necessary in practice. See also
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1244959#c8
  if (net::registry_controlled_domains::SameDomainOrHost(
          appid_url, origin,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    return appid;
  }

  // As a compatibility hack, sites within google.com are allowed to assert two
  // special-case AppIDs. Firefox also does this:
  // https://groups.google.com/forum/#!msg/mozilla.dev.platform/Uiu3fwnA2xw/201ynAiPAQAJ
  const GURL kGstatic1 =
      GURL("https://www.gstatic.com/securitykey/origins.json");
  const GURL kGstatic2 =
      GURL("https://www.gstatic.com/securitykey/a/google.com/origins.json");
  DCHECK(kGstatic1.is_valid() && kGstatic2.is_valid());

  if (origin.DomainIs("google.com") && !appid_url.has_ref() &&
      (appid_url.EqualsIgnoringRef(kGstatic1) ||
       appid_url.EqualsIgnoringRef(kGstatic2))) {
    return appid;
  }

  ReportSecurityCheckFailure(
      RelyingPartySecurityCheckFailure::kAppIdExtensionDomainMismatch);

  return base::nullopt;
}

// The application parameter is the SHA-256 hash of the UTF-8 encoding of
// the application identity (i.e. relying_party_id) of the application
// requesting the registration.
std::array<uint8_t, crypto::kSHA256Length> CreateApplicationParameter(
    const std::string& relying_party_id) {
  std::array<uint8_t, crypto::kSHA256Length> application_parameter;
  crypto::SHA256HashString(relying_party_id, application_parameter.data(),
                           application_parameter.size());
  return application_parameter;
}

device::CtapGetAssertionRequest CreateCtapGetAssertionRequest(
    const std::string& client_data_json,
    const blink::mojom::PublicKeyCredentialRequestOptionsPtr& options,
    base::Optional<std::string> app_id,
    bool is_incognito) {
  device::CtapGetAssertionRequest request_parameter(options->relying_party_id,
                                                    client_data_json);

  request_parameter.allow_list = options->allow_credentials;

  request_parameter.user_verification = options->user_verification;

  if (app_id) {
    request_parameter.alternative_application_parameter =
        CreateApplicationParameter(*app_id);
    request_parameter.app_id = std::move(*app_id);
  }

  if (!options->cable_authentication_data.empty()) {
    request_parameter.cable_extension = options->cable_authentication_data;
  }
  request_parameter.is_incognito_mode = is_incognito;
  return request_parameter;
}

// Parses the FIDO transport types extension from the DER-encoded, X.509
// certificate in |der_cert| and appends any unique transport types found to
// |out_transports|.
void AppendUniqueTransportsFromCertificate(
    base::span<const uint8_t> der_cert,
    std::vector<device::FidoTransportProtocol>* out_transports) {
  // See
  // https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-authenticator-transports-extension-v1.2-ps-20170411.html#fido-u2f-certificate-transports-extension
  static constexpr uint8_t kTransportTypesOID[] = {
      0x2b, 0x06, 0x01, 0x04, 0x01, 0x82, 0xe5, 0x1c, 0x02, 0x01, 0x01};
  bool present, critical;
  base::StringPiece contents;
  if (!net::asn1::ExtractExtensionFromDERCert(
          base::StringPiece(reinterpret_cast<const char*>(der_cert.data()),
                            der_cert.size()),
          base::StringPiece(reinterpret_cast<const char*>(kTransportTypesOID),
                            sizeof(kTransportTypesOID)),
          &present, &critical, &contents) ||
      !present) {
    return;
  }

  const net::der::Input contents_der(contents);
  net::der::Parser contents_parser(contents_der);
  net::der::BitString transport_bits;
  if (!contents_parser.ReadBitString(&transport_bits)) {
    return;
  }

  // The certificate extension contains a BIT STRING where different bits
  // indicate support for different transports. The following array maps
  // between these bit indexes and the FidoTransportProtocol enum.
  static constexpr struct {
    uint8_t bit_index;
    device::FidoTransportProtocol transport;
  } kTransportMapping[] = {
      // Bit 0 is "Bluetooth Classic", not BLE. Since webauthn doesn't define a
      // transport type for this we ignore it.
      {1, device::FidoTransportProtocol::kBluetoothLowEnergy},
      {2, device::FidoTransportProtocol::kUsbHumanInterfaceDevice},
      {3, device::FidoTransportProtocol::kNearFieldCommunication},
      {4, device::FidoTransportProtocol::kInternal},
  };

  for (const auto& mapping : kTransportMapping) {
    if (transport_bits.AssertsBit(mapping.bit_index) &&
        !base::Contains(*out_transports, mapping.transport)) {
      out_transports->push_back(mapping.transport);
    }
  }
}

enum class AttestationErasureOption {
  kIncludeAttestation,
  kEraseAttestationButIncludeAaguid,
  kEraseAttestationAndAaguid,
};

blink::mojom::MakeCredentialAuthenticatorResponsePtr
CreateMakeCredentialResponse(
    const std::string& client_data_json,
    device::AuthenticatorMakeCredentialResponse response_data,
    AttestationErasureOption attestation_erasure) {
  auto response = blink::mojom::MakeCredentialAuthenticatorResponse::New();
  auto common_info = blink::mojom::CommonCredentialInfo::New();
  common_info->client_data_json.assign(client_data_json.begin(),
                                       client_data_json.end());
  common_info->raw_id = response_data.raw_credential_id();
  common_info->id = response_data.GetId();
  response->info = std::move(common_info);

  // The transport list must not contain duplicates but the order doesn't matter
  // because Blink will sort the resulting strings before returning them.
  std::vector<device::FidoTransportProtocol> transports;
  if (response_data.transport_used()) {
    transports.push_back(*response_data.transport_used());
  }
  // If the attestation certificate specifies that the token supports any other
  // transports, include them in the list.
  base::Optional<base::span<const uint8_t>> leaf_cert =
      response_data.attestation_object()
          .attestation_statement()
          .GetLeafCertificate();
  if (leaf_cert) {
    AppendUniqueTransportsFromCertificate(*leaf_cert, &transports);
  }

  response->transports = std::move(transports);

  const base::Optional<cbor::Value>& maybe_extensions =
      response_data.attestation_object().authenticator_data().extensions();
  if (maybe_extensions) {
    DCHECK(maybe_extensions->is_map());
    const cbor::Value::MapValue& extensions = maybe_extensions->GetMap();
    const auto hmac_secret_it =
        extensions.find(cbor::Value(device::kExtensionHmacSecret));
    if (hmac_secret_it != extensions.end() &&
        hmac_secret_it->second.is_bool()) {
      response->echo_hmac_create_secret = true;
      response->hmac_create_secret = hmac_secret_it->second.GetBool();
    }
  }

  switch (attestation_erasure) {
    case AttestationErasureOption::kIncludeAttestation:
      break;
    case AttestationErasureOption::kEraseAttestationButIncludeAaguid:
      response_data.EraseAttestationStatement(
          device::AttestationObject::AAGUID::kInclude);
      break;
    case AttestationErasureOption::kEraseAttestationAndAaguid:
      response_data.EraseAttestationStatement(
          device::AttestationObject::AAGUID::kErase);
      break;
  }
  response->attestation_object =
      response_data.GetCBOREncodedAttestationObject();

  return response;
}

blink::mojom::GetAssertionAuthenticatorResponsePtr CreateGetAssertionResponse(
    const std::string& client_data_json,
    device::AuthenticatorGetAssertionResponse response_data,
    base::Optional<bool> echo_appid_extension) {
  auto response = blink::mojom::GetAssertionAuthenticatorResponse::New();
  auto common_info = blink::mojom::CommonCredentialInfo::New();
  common_info->client_data_json.assign(client_data_json.begin(),
                                       client_data_json.end());
  common_info->raw_id = response_data.raw_credential_id();
  common_info->id = response_data.GetId();
  response->info = std::move(common_info);
  response->authenticator_data =
      response_data.auth_data().SerializeToByteArray();
  response->signature = response_data.signature();
  if (echo_appid_extension) {
    response->echo_appid_extension = true;
    response->appid_extension = *echo_appid_extension;
  }
  response_data.user_entity()
      ? response->user_handle.emplace(response_data.user_entity()->id)
      : response->user_handle.emplace();
  return response;
}

std::string Base64UrlEncode(const base::span<const uint8_t> input) {
  std::string ret;
  base::Base64UrlEncode(
      base::StringPiece(reinterpret_cast<const char*>(input.data()),
                        input.size()),
      base::Base64UrlEncodePolicy::OMIT_PADDING, &ret);
  return ret;
}

base::flat_set<device::FidoTransportProtocol> GetTransportsEnabledByFlags() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableWebAuthTestingAPI)) {
    return device::GetAllTransportProtocols();
  }
  base::flat_set<device::FidoTransportProtocol> transports;
  transports.insert(device::FidoTransportProtocol::kUsbHumanInterfaceDevice);
  transports.insert(device::FidoTransportProtocol::kInternal);

  // TODO(crbug.com/885165): We should not directly access the BLE stack here.
  // It is used by //device/fido, so its availability should be checked there.
  if (!device::BluetoothAdapterFactory::Get().IsLowEnergySupported())
    return transports;

  if (base::FeatureList::IsEnabled(features::kWebAuthBle)) {
    transports.insert(device::FidoTransportProtocol::kBluetoothLowEnergy);
  }

  // caBLE is independent of the BLE transport.
  if (base::FeatureList::IsEnabled(features::kWebAuthCable) ||
      base::FeatureList::IsEnabled(device::kWebAuthPhoneSupport)) {
    transports.insert(
        device::FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy);
  }

  return transports;
}

// Returns the transports to be used for a request made by |caller_origin|.
base::flat_set<device::FidoTransportProtocol> GetTransports(
    url::Origin caller_origin,
    base::flat_set<device::FidoTransportProtocol> available_transports) {
  // U2F requests proxied from the cryptotoken extension are limited to USB
  // devices.
  return OriginIsCryptoTokenExtension(caller_origin)
             ? base::flat_set<device::FidoTransportProtocol>(
                   {device::FidoTransportProtocol::kUsbHumanInterfaceDevice})
             : available_transports;
}

}  // namespace

AuthenticatorCommon::AuthenticatorCommon(
    RenderFrameHost* render_frame_host,
    service_manager::Connector* connector,
    std::unique_ptr<base::OneShotTimer> timer)
    : render_frame_host_(render_frame_host),
      connector_(connector),
      transports_(GetTransportsEnabledByFlags()),
      timer_(std::move(timer)) {
  DCHECK(render_frame_host_);
  DCHECK(timer_);
  // Disable the back-forward cache for any document that makes WebAuthn
  // requests. Pages using privacy-sensitive APIs are generally exempt from
  // back-forward cache for now as a precaution.
  BackForwardCache::DisableForRenderFrameHost(render_frame_host,
                                              "WebAuthenticationAPI");
}

AuthenticatorCommon::~AuthenticatorCommon() {
  // This call exists to assert that |render_frame_host_| outlives this object.
  // If this is violated, ASAN should notice.
  render_frame_host_->GetRoutingID();
}

std::unique_ptr<AuthenticatorRequestClientDelegate>
AuthenticatorCommon::CreateRequestDelegate(std::string relying_party_id) {
  DCHECK(!relying_party_id.empty());
  auto* frame_tree_node =
      static_cast<RenderFrameHostImpl*>(render_frame_host_)->frame_tree_node();
  if (AuthenticatorEnvironmentImpl::GetInstance()->GetVirtualFactoryFor(
          frame_tree_node)) {
    return std::make_unique<VirtualAuthenticatorRequestDelegate>(
        frame_tree_node);
  }
  return GetContentClient()->browser()->GetWebAuthenticationRequestDelegate(
      render_frame_host_, relying_party_id_);
}

void AuthenticatorCommon::StartMakeCredentialRequest(
    bool allow_skipping_pin_touch) {
  discovery_factory_ =
      AuthenticatorEnvironmentImpl::GetInstance()->GetDiscoveryFactoryOverride(
          static_cast<RenderFrameHostImpl*>(render_frame_host_)
              ->frame_tree_node());
  if (!discovery_factory_) {
    discovery_factory_ = request_delegate_->GetDiscoveryFactory();
  }

  if (base::FeatureList::IsEnabled(device::kWebAuthPhoneSupport)) {
    std::vector<device::CableDiscoveryData> cable_pairings =
        request_delegate_->GetCablePairings();
    const bool have_paired_phones = !cable_pairings.empty();

    device::QRGeneratorKey qr_generator_key(
        device::CableDiscoveryData::NewQRKey());
    if (request_delegate_->SetCableTransportInfo(
            /*cable_extension_provided=*/false, have_paired_phones,
            qr_generator_key)) {
      discovery_factory_->set_cable_data(cable_pairings,
                                         std::move(qr_generator_key));
    }
  }

  request_ = std::make_unique<device::MakeCredentialRequestHandler>(
      connector_, discovery_factory_,
      GetTransports(caller_origin_, transports_),
      *ctap_make_credential_request_, *authenticator_selection_criteria_,
      allow_skipping_pin_touch,
      base::BindOnce(&AuthenticatorCommon::OnRegisterResponse,
                     weak_factory_.GetWeakPtr()));

  request_delegate_->RegisterActionCallbacks(
      base::BindOnce(&AuthenticatorCommon::OnCancelFromUI,
                     weak_factory_.GetWeakPtr()) /* cancel_callback */,
      base::BindRepeating(
          &AuthenticatorCommon::StartMakeCredentialRequest,
          weak_factory_.GetWeakPtr(),
          /*allow_skipping_pin_touch=*/false) /* start_over_callback */,
      base::BindRepeating(
          &device::FidoRequestHandlerBase::StartAuthenticatorRequest,
          request_->GetWeakPtr()) /* request_callback */,
      base::BindRepeating(
          &device::FidoRequestHandlerBase::PowerOnBluetoothAdapter,
          request_->GetWeakPtr()) /* bluetooth_adapter_power_on_callback */,
      base::BindRepeating(
          &device::FidoRequestHandlerBase::InitiatePairingWithDevice,
          request_->GetWeakPtr()) /* ble_pairing_callback */);
  if (authenticator_selection_criteria_->require_resident_key()) {
    request_delegate_->SetMightCreateResidentCredential(true);
  }
  request_->set_observer(request_delegate_.get());
}

void AuthenticatorCommon::StartGetAssertionRequest(
    bool allow_skipping_pin_touch) {
  discovery_factory_ =
      AuthenticatorEnvironmentImpl::GetInstance()->GetDiscoveryFactoryOverride(
          static_cast<RenderFrameHostImpl*>(render_frame_host_)
              ->frame_tree_node());
  if (!discovery_factory_) {
    discovery_factory_ = request_delegate_->GetDiscoveryFactory();
  }

  std::vector<device::CableDiscoveryData> cable_pairings;
  bool have_cable_extension = false;
  if (ctap_get_assertion_request_->cable_extension &&
      request_delegate_->ShouldPermitCableExtension(caller_origin_) &&
      IsFocused()) {
    cable_pairings = *ctap_get_assertion_request_->cable_extension;
    have_cable_extension = !cable_pairings.empty();
  }

  base::Optional<device::QRGeneratorKey> qr_generator_key;
  bool have_paired_phones = false;
  if (base::FeatureList::IsEnabled(device::kWebAuthPhoneSupport)) {
    qr_generator_key.emplace(device::CableDiscoveryData::NewQRKey());
    auto paired_phones = request_delegate_->GetCablePairings();
    have_paired_phones = !paired_phones.empty();
    cable_pairings.insert(cable_pairings.end(), paired_phones.begin(),
                          paired_phones.end());
  }

  if ((!cable_pairings.empty() || qr_generator_key.has_value()) &&
      request_delegate_->SetCableTransportInfo(
          have_cable_extension, have_paired_phones, qr_generator_key)) {
    discovery_factory_->set_cable_data(std::move(cable_pairings),
                                       std::move(qr_generator_key));
  }

  request_ = std::make_unique<device::GetAssertionRequestHandler>(
      connector_, discovery_factory_,
      GetTransports(caller_origin_, transports_), *ctap_get_assertion_request_,
      allow_skipping_pin_touch,
      base::BindOnce(&AuthenticatorCommon::OnSignResponse,
                     weak_factory_.GetWeakPtr()));

  request_delegate_->RegisterActionCallbacks(
      base::BindOnce(&AuthenticatorCommon::OnCancelFromUI,
                     weak_factory_.GetWeakPtr()) /* cancel_callback */,
      base::BindRepeating(
          &AuthenticatorCommon::StartGetAssertionRequest,
          weak_factory_.GetWeakPtr(),
          /*allow_skipping_pin_touch=*/false) /* start_over_callback */,
      base::BindRepeating(
          &device::FidoRequestHandlerBase::StartAuthenticatorRequest,
          request_->GetWeakPtr()) /* request_callback */,
      base::BindRepeating(
          &device::FidoRequestHandlerBase::PowerOnBluetoothAdapter,
          request_->GetWeakPtr()) /* bluetooth_adapter_power_on_callback */,
      base::BindRepeating(
          &device::FidoRequestHandlerBase::InitiatePairingWithDevice,
          request_->GetWeakPtr()) /* ble_pairing_callback*/);

  request_->set_observer(request_delegate_.get());
}

bool AuthenticatorCommon::IsFocused() const {
  return render_frame_host_->IsCurrent() && request_delegate_->IsFocused();
}

// static
std::string AuthenticatorCommon::SerializeCollectedClientDataToJson(
    const std::string& type,
    const std::string& origin,
    base::span<const uint8_t> challenge,
    bool use_legacy_u2f_type_key /* = false */) {
  static constexpr char kChallengeKey[] = "challenge";
  static constexpr char kOriginKey[] = "origin";

  base::DictionaryValue client_data;
  client_data.SetKey(use_legacy_u2f_type_key ? "typ" : "type",
                     base::Value(type));
  client_data.SetKey(kChallengeKey, base::Value(Base64UrlEncode(challenge)));
  client_data.SetKey(kOriginKey, base::Value(origin));

  if (base::RandDouble() < 0.2) {
    // An extra key is sometimes added to ensure that RPs do not make
    // unreasonably specific assumptions about the clientData JSON. This is
    // done in the fashion of
    // https://tools.ietf.org/html/draft-davidben-tls-grease-01
    client_data.SetKey("extra_keys_may_be_added_here",
                       base::Value("do not compare clientDataJSON against a "
                                   "template. See https://goo.gl/yabPex"));
  }

  std::string json;
  base::JSONWriter::Write(client_data, &json);
  return json;
}

// mojom::Authenticator
void AuthenticatorCommon::MakeCredential(
    url::Origin caller_origin,
    blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
    blink::mojom::Authenticator::MakeCredentialCallback callback) {
  if (request_) {
    if (OriginIsCryptoTokenExtension(caller_origin)) {
      // Requests originating from cryptotoken will generally outlive any
      // navigation events on the tab of the request's sender. Evict pending
      // requests if cryptotoken sends a new one such that requests from before
      // a navigation event do not prevent new requests. See
      // https://crbug.com/935480.
      CancelWithStatus(blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
    } else {
      std::move(callback).Run(
          blink::mojom::AuthenticatorStatus::PENDING_REQUEST, nullptr);
      return;
    }
  }
  DCHECK(!request_);

  blink::mojom::AuthenticatorStatus domain_validation =
      ValidateEffectiveDomain(caller_origin);
  if (domain_validation != blink::mojom::AuthenticatorStatus::SUCCESS) {
    ReportSecurityCheckFailure(
        RelyingPartySecurityCheckFailure::kOpaqueOrNonSecureOrigin);
    InvokeCallbackAndCleanup(std::move(callback), domain_validation, nullptr,
                             Focus::kDontCheck);
    return;
  }

  if (!IsRelyingPartyIdValid(options->relying_party.id, caller_origin)) {
    ReportSecurityCheckFailure(
        RelyingPartySecurityCheckFailure::kRelyingPartyIdInvalid);
    InvokeCallbackAndCleanup(
        std::move(callback),
        blink::mojom::AuthenticatorStatus::BAD_RELYING_PARTY_ID, nullptr,
        Focus::kDontCheck);
    return;
  }

  base::Optional<std::string> appid_exclude;
  if (options->appid_exclude) {
    appid_exclude =
        ProcessAppIdExtension(*options->appid_exclude, caller_origin);
    if (!appid_exclude) {
      InvokeCallbackAndCleanup(
          std::move(callback),
          blink::mojom::AuthenticatorStatus::INVALID_DOMAIN, nullptr,
          Focus::kDontCheck);
      return;
    }
  }

  if (!IsAPrioriAuthenticatedUrl(options->user.icon_url) ||
      !IsAPrioriAuthenticatedUrl(options->relying_party.icon_url)) {
    ReportSecurityCheckFailure(
        RelyingPartySecurityCheckFailure::kIconUrlInvalid);
    bad_message::ReceivedBadMessage(render_frame_host_->GetProcess(),
                                    bad_message::AUTH_INVALID_ICON_URL);
    InvokeCallbackAndCleanup(
        std::move(callback),
        blink::mojom::AuthenticatorStatus::INVALID_ICON_URL, nullptr,
        Focus::kDontCheck);
    return;
  }

  caller_origin_ = caller_origin;
  relying_party_id_ = options->relying_party.id;

  request_delegate_ = CreateRequestDelegate(relying_party_id_);
  if (!request_delegate_) {
    InvokeCallbackAndCleanup(std::move(callback),
                             blink::mojom::AuthenticatorStatus::PENDING_REQUEST,
                             nullptr, Focus::kDontCheck);
    return;
  }

  if (!IsFocused()) {
    InvokeCallbackAndCleanup(std::move(callback),
                             blink::mojom::AuthenticatorStatus::NOT_FOCUSED);
    return;
  }

  bool resident_key = options->authenticator_selection &&
                      options->authenticator_selection->require_resident_key();
  if (resident_key && !request_delegate_->SupportsResidentKeys()) {
    // Disallow the creation of resident credentials.
    InvokeCallbackAndCleanup(
        std::move(callback),
        blink::mojom::AuthenticatorStatus::RESIDENT_CREDENTIALS_UNSUPPORTED);
    return;
  }

  authenticator_selection_criteria_ =
      options->authenticator_selection
          ? options->authenticator_selection
          : device::AuthenticatorSelectionCriteria();

  // Reject any non-sensical credProtect extension values.
  if (  // Can't require the default policy (or no policy).
      (options->enforce_protection_policy &&
       (options->protection_policy ==
            blink::mojom::ProtectionPolicy::UNSPECIFIED ||
        options->protection_policy == blink::mojom::ProtectionPolicy::NONE)) ||
      // For non-resident keys, NONE doesn't make sense. (UV_OR_CRED_ID_REQUIRED
      // does because, with CTAP 2.0, just because a resident key isn't
      // _required_ doesn't mean that one won't be created and an RP might want
      // credProtect to take effect if that happens.)
      (!resident_key &&
       options->protection_policy == blink::mojom::ProtectionPolicy::NONE) ||
      // UV_REQUIRED only makes sense if UV is required overall.
      (options->protection_policy ==
           blink::mojom::ProtectionPolicy::UV_REQUIRED &&
       authenticator_selection_criteria_->user_verification_requirement() !=
           device::UserVerificationRequirement::kRequired)) {
    InvokeCallbackAndCleanup(
        std::move(callback),
        blink::mojom::AuthenticatorStatus::PROTECTION_POLICY_INCONSISTENT);
    return;
  }

  if (options->protection_policy ==
          blink::mojom::ProtectionPolicy::UNSPECIFIED &&
      resident_key) {
    // If not specified, UV_OR_CRED_ID_REQUIRED is made the default.
    options->protection_policy =
        blink::mojom::ProtectionPolicy::UV_OR_CRED_ID_REQUIRED;
  }

  DCHECK(make_credential_response_callback_.is_null());
  make_credential_response_callback_ = std::move(callback);

  timer_->Start(
      FROM_HERE, options->adjusted_timeout,
      base::BindOnce(&AuthenticatorCommon::OnTimeout, base::Unretained(this)));
  if (!connector_)
    connector_ = GetSystemConnector();

  // Save client data to return with the authenticator response.
  // TODO(kpaulhamus): Fetch and add the Channel ID/Token Binding ID public key
  // used to communicate with the origin.
  if (OriginIsCryptoTokenExtension(caller_origin_)) {
    // Cryptotoken requests should be proxied without UI.
    request_delegate_->DisableUI();
    // As Cryptotoken validates the origin, accept the relying party id as the
    // origin from requests originating from Cryptotoken. The origin is provided
    // in Cryptotoken requests as the relying party name, which should be used
    // as part of client data.
    client_data_json_ = SerializeCollectedClientDataToJson(
        client_data::kU2fRegisterType, *options->relying_party.name,
        std::move(options->challenge), true /* use_legacy_u2f_type_key */);
  } else {
    client_data_json_ = SerializeCollectedClientDataToJson(
        client_data::kCreateType, caller_origin_.Serialize(),
        std::move(options->challenge));
  }

  UMA_HISTOGRAM_COUNTS_100(
      "WebAuthentication.MakeCredentialExcludeCredentialsCount",
      options->exclude_credentials.size());

  ctap_make_credential_request_ = device::CtapMakeCredentialRequest(
      client_data_json_, options->relying_party, options->user,
      device::PublicKeyCredentialParams(options->public_key_parameters));
  ctap_make_credential_request_->exclude_list = options->exclude_credentials;
  ctap_make_credential_request_->hmac_secret = options->hmac_create_secret;
  ctap_make_credential_request_->app_id = std::move(appid_exclude);
  ctap_make_credential_request_->is_incognito_mode =
      browser_context()->IsOffTheRecord();
  // On dual protocol CTAP2/U2F devices, force credential creation over U2F.
  ctap_make_credential_request_->is_u2f_only =
      OriginIsCryptoTokenExtension(caller_origin_);

  // Compute the effective attestation conveyance preference and set
  // |attestation_requested_| for showing the attestation consent prompt later.
  ::device::AttestationConveyancePreference attestation = options->attestation;
  if (attestation == ::device::AttestationConveyancePreference::kEnterprise &&
      !request_delegate_->ShouldPermitIndividualAttestation(
          relying_party_id_)) {
    attestation = ::device::AttestationConveyancePreference::kDirect;
  }
  ctap_make_credential_request_->attestation_preference = attestation;
  attestation_requested_ =
      attestation != ::device::AttestationConveyancePreference::kNone;

  switch (options->protection_policy) {
    case blink::mojom::ProtectionPolicy::UNSPECIFIED:
    case blink::mojom::ProtectionPolicy::NONE:
      break;
    case blink::mojom::ProtectionPolicy::UV_OR_CRED_ID_REQUIRED:
      ctap_make_credential_request_->cred_protect =
          std::make_pair(device::CredProtect::kUVOrCredIDRequired,
                         options->enforce_protection_policy);
      break;
    case blink::mojom::ProtectionPolicy::UV_REQUIRED:
      ctap_make_credential_request_->cred_protect = std::make_pair(
          device::CredProtect::kUVRequired, options->enforce_protection_policy);
      break;
  }

  StartMakeCredentialRequest(/*allow_skipping_pin_touch=*/true);
}

// mojom:Authenticator
void AuthenticatorCommon::GetAssertion(
    url::Origin caller_origin,
    blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
    blink::mojom::Authenticator::GetAssertionCallback callback) {
  if (request_) {
    if (OriginIsCryptoTokenExtension(caller_origin)) {
      // Requests originating from cryptotoken will generally outlive any
      // navigation events on the tab of the request's sender. Evict pending
      // requests if cryptotoken sends a new one such that requests from before
      // a navigation event do not prevent new requests. See
      // https://crbug.com/935480.
      CancelWithStatus(blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
    } else {
      std::move(callback).Run(
          blink::mojom::AuthenticatorStatus::PENDING_REQUEST, nullptr);
      return;
    }
  }
  DCHECK(!request_);

  blink::mojom::AuthenticatorStatus domain_validation =
      ValidateEffectiveDomain(caller_origin);
  if (domain_validation != blink::mojom::AuthenticatorStatus::SUCCESS) {
    ReportSecurityCheckFailure(
        RelyingPartySecurityCheckFailure::kOpaqueOrNonSecureOrigin);
    InvokeCallbackAndCleanup(std::move(callback), domain_validation, nullptr);
    return;
  }

  if (!IsRelyingPartyIdValid(options->relying_party_id, caller_origin)) {
    ReportSecurityCheckFailure(
        RelyingPartySecurityCheckFailure::kRelyingPartyIdInvalid);
    InvokeCallbackAndCleanup(
        std::move(callback),
        blink::mojom::AuthenticatorStatus::BAD_RELYING_PARTY_ID, nullptr);
    return;
  }

  caller_origin_ = caller_origin;
  relying_party_id_ = options->relying_party_id;

  request_delegate_ = CreateRequestDelegate(relying_party_id_);
  if (!request_delegate_) {
    InvokeCallbackAndCleanup(std::move(callback),
                             blink::mojom::AuthenticatorStatus::PENDING_REQUEST,
                             nullptr);
    return;
  }

  // Save client data to return with the authenticator response.
  // TODO(kpaulhamus): Fetch and add the Channel ID/Token Binding ID public key
  // used to communicate with the origin.
  if (OriginIsCryptoTokenExtension(caller_origin)) {
    request_delegate_->DisableUI();

    // As Cryptotoken validates the origin, accept the relying party id as the
    // origin from requests originating from Cryptotoken.
    client_data_json_ = SerializeCollectedClientDataToJson(
        client_data::kU2fSignType, options->relying_party_id,
        std::move(options->challenge), true /* use_legacy_u2f_type_key */);
  } else {
    client_data_json_ = SerializeCollectedClientDataToJson(
        client_data::kGetType, caller_origin_.Serialize(),
        std::move(options->challenge));
  }

  if (options->allow_credentials.empty()) {
    if (!request_delegate_->SupportsResidentKeys()) {
      InvokeCallbackAndCleanup(
          std::move(callback),
          blink::mojom::AuthenticatorStatus::RESIDENT_CREDENTIALS_UNSUPPORTED);
      return;
    }
    empty_allow_list_ = true;
  }

  if (options->appid) {
    app_id_ = ProcessAppIdExtension(*options->appid, caller_origin_);
    if (!app_id_) {
      std::move(callback).Run(blink::mojom::AuthenticatorStatus::INVALID_DOMAIN,
                              nullptr);
      return;
    }
  }

  UMA_HISTOGRAM_COUNTS_100(
      "WebAuthentication.CredentialRequestAllowCredentialsCount",
      options->allow_credentials.size());

  DCHECK(get_assertion_response_callback_.is_null());
  get_assertion_response_callback_ = std::move(callback);

  timer_->Start(
      FROM_HERE, options->adjusted_timeout,
      base::BindOnce(&AuthenticatorCommon::OnTimeout, base::Unretained(this)));

  if (!connector_)
    connector_ = GetSystemConnector();

  ctap_get_assertion_request_ = CreateCtapGetAssertionRequest(
      client_data_json_, std::move(options), app_id_,
      browser_context()->IsOffTheRecord());
  ctap_get_assertion_request_->is_u2f_only =
      OriginIsCryptoTokenExtension(caller_origin_);

  StartGetAssertionRequest(/*allow_skipping_pin_touch=*/true);
}

void AuthenticatorCommon::IsUserVerifyingPlatformAuthenticatorAvailable(
    blink::mojom::Authenticator::
        IsUserVerifyingPlatformAuthenticatorAvailableCallback callback) {
  const std::string relying_party_id =
      render_frame_host_->GetLastCommittedOrigin().host();
  // Use |request_delegate_| if a request is currently in progress; or create a
  // temporary request delegate otherwise.
  //
  // Note that |CreateRequestDelegate| may return nullptr if there is an active
  // |request_delegate_| already.
  std::unique_ptr<AuthenticatorRequestClientDelegate> maybe_request_delegate =
      request_delegate_ ? nullptr : CreateRequestDelegate(relying_party_id);
  AuthenticatorRequestClientDelegate* request_delegate_ptr =
      request_delegate_ ? request_delegate_.get()
                        : maybe_request_delegate.get();

  const bool result =
      request_delegate_ptr->IsUserVerifyingPlatformAuthenticatorAvailable();

  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

void AuthenticatorCommon::Cancel() {
  CancelWithStatus(blink::mojom::AuthenticatorStatus::ABORT_ERROR);
}

// Callback to handle the async registration response from a U2fDevice.
void AuthenticatorCommon::OnRegisterResponse(
    device::MakeCredentialStatus status_code,
    base::Optional<device::AuthenticatorMakeCredentialResponse> response_data,
    const device::FidoAuthenticator* authenticator) {
  if (!request_) {
    // Either the callback was called immediately and |request_| has not yet
    // been assigned (this is a bug), or a navigation caused the request to be
    // canceled while a callback was enqueued.
    return;
  }

  switch (status_code) {
    case device::MakeCredentialStatus::kUserConsentButCredentialExcluded:
      // Duplicate registration: the new credential would be created on an
      // authenticator that already contains one of the credentials in
      // |exclude_credentials|.
      SignalFailureToRequestDelegate(
          authenticator,
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kKeyAlreadyRegistered,
          blink::mojom::AuthenticatorStatus::CREDENTIAL_EXCLUDED);
      return;
    case device::MakeCredentialStatus::kAuthenticatorResponseInvalid:
      // The response from the authenticator was corrupted.
      InvokeCallbackAndCleanup(
          std::move(make_credential_response_callback_),
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR, nullptr,
          Focus::kDoCheck);
      return;
    case device::MakeCredentialStatus::kUserConsentDenied:
      SignalFailureToRequestDelegate(
          authenticator,
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kUserConsentDenied,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::MakeCredentialStatus::kSoftPINBlock:
      SignalFailureToRequestDelegate(
          authenticator,
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kSoftPINBlock,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::MakeCredentialStatus::kHardPINBlock:
      SignalFailureToRequestDelegate(
          authenticator,
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kHardPINBlock,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::MakeCredentialStatus::kAuthenticatorRemovedDuringPINEntry:
      SignalFailureToRequestDelegate(
          authenticator,
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kAuthenticatorRemovedDuringPINEntry,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::MakeCredentialStatus::kAuthenticatorMissingResidentKeys:
      SignalFailureToRequestDelegate(
          authenticator,
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kAuthenticatorMissingResidentKeys,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::MakeCredentialStatus::kAuthenticatorMissingUserVerification:
      SignalFailureToRequestDelegate(
          authenticator,
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kAuthenticatorMissingUserVerification,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::MakeCredentialStatus::kStorageFull:
      SignalFailureToRequestDelegate(
          authenticator,
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kStorageFull,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::MakeCredentialStatus::kWinInvalidStateError:
      InvokeCallbackAndCleanup(
          std::move(make_credential_response_callback_),
          blink::mojom::AuthenticatorStatus::CREDENTIAL_EXCLUDED, nullptr,
          Focus::kDoCheck);
      return;
    case device::MakeCredentialStatus::kWinNotAllowedError:
      InvokeCallbackAndCleanup(
          std::move(make_credential_response_callback_),
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR, nullptr,
          Focus::kDoCheck);
      return;
    case device::MakeCredentialStatus::kSuccess:
      DCHECK(response_data.has_value());
      DCHECK(authenticator);

      auto transport_used = authenticator->AuthenticatorTransport();
      if (transport_used) {
        request_delegate_->UpdateLastTransportUsed(*transport_used);
      }
      bool is_transport_used_internal =
          transport_used &&
          *transport_used == device::FidoTransportProtocol::kInternal;
      if (attestation_requested_) {
        // Cryptotoken requests may bypass the attestation prompt because the
        // extension implements its own. Invoking the attestation prompt code
        // here would not work anyway, because the WebContents associated with
        // the extension is not associated with any tab and therefore cannot
        // draw modal dialogs for the UI.
        //
        // Note that for AttestationConveyancePreference::kNone, attestation
        // erasure is still performed as usual.
        if (OriginIsCryptoTokenExtension(caller_origin_)) {
          InvokeCallbackAndCleanup(
              std::move(make_credential_response_callback_),
              blink::mojom::AuthenticatorStatus::SUCCESS,
              CreateMakeCredentialResponse(
                  std::move(client_data_json_), std::move(*response_data),
                  AttestationErasureOption::kIncludeAttestation),
              Focus::kDoCheck);
          return;
        }

        UMA_HISTOGRAM_ENUMERATION("WebAuthentication.AttestationPromptResult",
                                  AttestationPromptResult::kQueried);
        awaiting_attestation_response_ = true;
        request_delegate_->ShouldReturnAttestation(
            relying_party_id_, authenticator,
            base::BindOnce(
                &AuthenticatorCommon::OnRegisterResponseAttestationDecided,
                weak_factory_.GetWeakPtr(), std::move(*response_data),
                is_transport_used_internal));
        return;
      }

      AttestationErasureOption attestation_erasure =
          AttestationErasureOption::kEraseAttestationAndAaguid;
      if (response_data->IsSelfAttestation()) {
        attestation_erasure = AttestationErasureOption::kIncludeAttestation;
      } else if (is_transport_used_internal) {
        // Contrary to what the WebAuthn spec says, for internal (platform)
        // authenticators we do not erase the AAGUID from authenticatorData,
        // even if requested attestationConveyancePreference is "none".
        attestation_erasure =
            AttestationErasureOption::kEraseAttestationButIncludeAaguid;
      }

      InvokeCallbackAndCleanup(
          std::move(make_credential_response_callback_),
          blink::mojom::AuthenticatorStatus::SUCCESS,
          CreateMakeCredentialResponse(std::move(client_data_json_),
                                       std::move(*response_data),
                                       attestation_erasure),
          Focus::kDoCheck);
      return;
  }
  NOTREACHED();
}

void AuthenticatorCommon::OnRegisterResponseAttestationDecided(
    device::AuthenticatorMakeCredentialResponse response_data,
    bool is_transport_used_internal,
    bool attestation_permitted) {
  awaiting_attestation_response_ = false;
  if (!request_) {
    // The request has already been cleaned up, probably because a navigation
    // occurred while the permissions prompt was pending.
    return;
  }

  DCHECK(attestation_requested_);

  AttestationErasureOption attestation_erasure;
  if (!attestation_permitted) {
    UMA_HISTOGRAM_ENUMERATION("WebAuthentication.AttestationPromptResult",
                              AttestationPromptResult::kBlocked);
    if (is_transport_used_internal) {
      // For internal (platform) authenticators, we do not erase the
      // AAGUID from authenticatorData even if the user declines to
      // share attestation.
      attestation_erasure =
          AttestationErasureOption::kEraseAttestationButIncludeAaguid;
    } else {
      attestation_erasure =
          AttestationErasureOption::kEraseAttestationAndAaguid;
    }
  } else {
    UMA_HISTOGRAM_ENUMERATION("WebAuthentication.AttestationPromptResult",
                              AttestationPromptResult::kAllowed);
    attestation_erasure = AttestationErasureOption::kIncludeAttestation;
  }

  // The check for IsAttestationCertificateInappropriatelyIdentifying is
  // performed after the permissions prompt, even though we know the answer
  // before, because this still effectively discloses the make & model of
  // the authenticator: If an RP sees a "none" attestation from Chrome after
  // requesting direct attestation then it knows that it was one of the
  // tokens with inappropriate certs.
  if (response_data.IsAttestationCertificateInappropriatelyIdentifying() &&
      !request_delegate_->ShouldPermitIndividualAttestation(
          relying_party_id_)) {
    // The attestation response is incorrectly individually identifiable, but
    // the consent is for make & model information about a token, not for
    // individually-identifiable information. Erase the attestation to stop it
    // begin a tracking signal.

    // The only way to get the underlying attestation will be to list the RP ID
    // in the enterprise policy, because that enables the individual attestation
    // bit in the register request and permits individual attestation generally.
    attestation_erasure = AttestationErasureOption::kEraseAttestationAndAaguid;
  }

  InvokeCallbackAndCleanup(std::move(make_credential_response_callback_),
                           blink::mojom::AuthenticatorStatus::SUCCESS,
                           CreateMakeCredentialResponse(
                               std::move(client_data_json_),
                               std::move(response_data), attestation_erasure),
                           Focus::kDoCheck);
}

void AuthenticatorCommon::OnSignResponse(
    device::GetAssertionStatus status_code,
    base::Optional<std::vector<device::AuthenticatorGetAssertionResponse>>
        response_data,
    const device::FidoAuthenticator* authenticator) {
  DCHECK(!response_data || !response_data->empty());  // empty vector is invalid

  if (!request_) {
    // Either the callback was called immediately and |request_| has not yet
    // been assigned (this is a bug), or a navigation caused the request to be
    // canceled while a callback was enqueued.
    return;
  }

  switch (status_code) {
    case device::GetAssertionStatus::kUserConsentButCredentialNotRecognized:
      SignalFailureToRequestDelegate(
          authenticator,
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kKeyNotRegistered,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::GetAssertionStatus::kAuthenticatorResponseInvalid:
      // The response from the authenticator was corrupted.
      InvokeCallbackAndCleanup(
          std::move(get_assertion_response_callback_),
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::GetAssertionStatus::kUserConsentDenied:
      SignalFailureToRequestDelegate(
          authenticator,
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kUserConsentDenied,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::GetAssertionStatus::kSoftPINBlock:
      SignalFailureToRequestDelegate(
          authenticator,
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kSoftPINBlock,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::GetAssertionStatus::kHardPINBlock:
      SignalFailureToRequestDelegate(
          authenticator,
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kHardPINBlock,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::GetAssertionStatus::kAuthenticatorRemovedDuringPINEntry:
      SignalFailureToRequestDelegate(
          authenticator,
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kAuthenticatorRemovedDuringPINEntry,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::GetAssertionStatus::kAuthenticatorMissingResidentKeys:
      SignalFailureToRequestDelegate(
          authenticator,
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kAuthenticatorMissingResidentKeys,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::GetAssertionStatus::kAuthenticatorMissingUserVerification:
      SignalFailureToRequestDelegate(
          authenticator,
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kAuthenticatorMissingUserVerification,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::GetAssertionStatus::kWinNotAllowedError:
      InvokeCallbackAndCleanup(
          std::move(get_assertion_response_callback_),
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::GetAssertionStatus::kSuccess:
      DCHECK(response_data.has_value());
      DCHECK(authenticator);

      if (authenticator->AuthenticatorTransport()) {
        request_delegate_->UpdateLastTransportUsed(
            *authenticator->AuthenticatorTransport());
      }

      // Show an account picker for requests with empty allow lists.
      // Authenticators may omit the identifying information in the user entity
      // if only one credential matches, or if they have account selection UI
      // built-in. In that case, consider that credential pre-selected.
      if (empty_allow_list_ &&
          (response_data->size() > 1 ||
           (response_data->at(0).user_entity() &&
            (response_data->at(0).user_entity()->name ||
             response_data->at(0).user_entity()->display_name)))) {
        request_delegate_->SelectAccount(
            std::move(*response_data),
            base::BindOnce(&AuthenticatorCommon::OnAccountSelected,
                           weak_factory_.GetWeakPtr()));
      } else {
        OnAccountSelected(std::move(response_data->at(0)));
      }
      return;
  }
  NOTREACHED();
}

void AuthenticatorCommon::OnAccountSelected(
    device::AuthenticatorGetAssertionResponse response) {
  base::Optional<bool> echo_appid_extension;
  if (app_id_) {
    echo_appid_extension =
        (response.GetRpIdHash() == CreateApplicationParameter(*app_id_));
  }
  InvokeCallbackAndCleanup(
      std::move(get_assertion_response_callback_),
      blink::mojom::AuthenticatorStatus::SUCCESS,
      CreateGetAssertionResponse(std::move(client_data_json_),
                                 std::move(response), echo_appid_extension));
  return;
}

void AuthenticatorCommon::SignalFailureToRequestDelegate(
    const ::device::FidoAuthenticator* authenticator,
    AuthenticatorRequestClientDelegate::InterestingFailureReason reason,
    blink::mojom::AuthenticatorStatus status) {
  error_awaiting_user_acknowledgement_ = status;

  // If WebAuthnUi is enabled, this error blocks until after receiving user
  // acknowledgement. Otherwise, the error is returned right away.
  if (request_delegate_->DoesBlockRequestOnFailure(reason)) {
    // Cancel pending authenticator requests before the error dialog is shown.
    request_->CancelActiveAuthenticators();
    return;
  }
  CancelWithStatus(error_awaiting_user_acknowledgement_);
}  // namespace content

// TODO(crbug.com/814418): Add web tests to verify timeouts are
// indistinguishable from NOT_ALLOWED_ERROR cases.
void AuthenticatorCommon::OnTimeout() {
  DCHECK(request_delegate_);
  if (awaiting_attestation_response_) {
    UMA_HISTOGRAM_ENUMERATION("WebAuthentication.AttestationPromptResult",
                              AttestationPromptResult::kTimeout);
    awaiting_attestation_response_ = false;
  }

  SignalFailureToRequestDelegate(
      /*authenticator=*/nullptr,
      AuthenticatorRequestClientDelegate::InterestingFailureReason::kTimeout,
      blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

void AuthenticatorCommon::CancelWithStatus(
    blink::mojom::AuthenticatorStatus status) {
  // If response callback is invoked already, then ignore cancel request.
  if (!make_credential_response_callback_ && !get_assertion_response_callback_)
    return;
  if (make_credential_response_callback_) {
    InvokeCallbackAndCleanup(std::move(make_credential_response_callback_),
                             status);
  } else if (get_assertion_response_callback_) {
    InvokeCallbackAndCleanup(std::move(get_assertion_response_callback_),
                             status);
  }
}

void AuthenticatorCommon::OnCancelFromUI() {
  CancelWithStatus(error_awaiting_user_acknowledgement_);
}

void AuthenticatorCommon::InvokeCallbackAndCleanup(
    blink::mojom::Authenticator::MakeCredentialCallback callback,
    blink::mojom::AuthenticatorStatus status,
    blink::mojom::MakeCredentialAuthenticatorResponsePtr response,
    Focus check_focus) {
  if (check_focus != Focus::kDontCheck && !(request_delegate_ && IsFocused())) {
    std::move(callback).Run(blink::mojom::AuthenticatorStatus::NOT_FOCUSED,
                            nullptr);
  } else {
    std::move(callback).Run(status, std::move(response));
  }

  Cleanup();
}

void AuthenticatorCommon::InvokeCallbackAndCleanup(
    blink::mojom::Authenticator::GetAssertionCallback callback,
    blink::mojom::AuthenticatorStatus status,
    blink::mojom::GetAssertionAuthenticatorResponsePtr response) {
  std::move(callback).Run(status, std::move(response));
  Cleanup();
}

void AuthenticatorCommon::Cleanup() {
  if (awaiting_attestation_response_) {
    UMA_HISTOGRAM_ENUMERATION("WebAuthentication.AttestationPromptResult",
                              AttestationPromptResult::kAbandoned);
    awaiting_attestation_response_ = false;
  }

  timer_->Stop();
  request_.reset();
  if (discovery_factory_) {
    // The FidoDiscoveryFactory instance may have been obtained via
    // AuthenticatorEnvironmentImpl::GetDiscoveryFactoryOverride() (in unit
    // tests or when WebDriver injected a virtual authenticator), in which case
    // it may be long-lived and handle more than one request. Hence, we need to
    // reset all per-request state before deleting its pointer.
    discovery_factory_->ResetRequestState();
    discovery_factory_ = nullptr;
  }
  request_delegate_.reset();
  make_credential_response_callback_.Reset();
  get_assertion_response_callback_.Reset();
  client_data_json_.clear();
  app_id_.reset();
  caller_origin_ = url::Origin();
  relying_party_id_.clear();
  attestation_requested_ = false;
  empty_allow_list_ = false;
  error_awaiting_user_acknowledgement_ =
      blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR;
}

BrowserContext* AuthenticatorCommon::browser_context() const {
  return content::WebContents::FromRenderFrameHost(render_frame_host_)
      ->GetBrowserContext();
}

}  // namespace content
