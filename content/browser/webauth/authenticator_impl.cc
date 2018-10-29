// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/authenticator_impl.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "base/base64url.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/string_piece.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/browser/bad_message.h"
#include "content/browser/webauth/authenticator_type_converters.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/origin_util.h"
#include "content/public/common/service_manager_connection.h"
#include "crypto/sha2.h"
#include "device/base/features.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/fido/attestation_statement.h"
#include "device/fido/authenticator_selection_criteria.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
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
#include "services/service_manager/public/cpp/connector.h"
#include "url/url_constants.h"
#include "url/url_util.h"

#if defined(OS_MACOSX)
#include "device/fido/mac/authenticator.h"
#endif

namespace content {

namespace client_data {
const char kCreateType[] = "webauthn.create";
const char kGetType[] = "webauthn.get";
}  // namespace client_data

namespace {

// AttestationPromptResult enumerates events related to attestation prompts.
// These values are recorded in an UMA histogram and so should not be
// reassigned.
enum class AttestationPromptResult {
  // kQueried indicates that the embedder was queried in order to determine
  // whether attestation information should be returned to the origin.
  kQueried = 0,
  // kTimeout indicates that a timeout occured while awaiting the result of an
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

// Ensure that the origin's effective domain is a valid domain.
// Only the domain format of host is valid.
// Reference https://url.spec.whatwg.org/#valid-domain-string and
// https://html.spec.whatwg.org/multipage/origin.html#concept-origin-effective-domain.
bool HasValidEffectiveDomain(url::Origin caller_origin) {
  return !caller_origin.opaque() &&
         !url::HostIsIPAddress(caller_origin.host()) &&
         content::IsOriginSecure(caller_origin.GetURL()) &&
         // Additionally, the scheme is required to be HTTP(S). Other schemes
         // may be supported in the future but the webauthn relying party is
         // just the domain of the origin so we would have to define how the
         // authority part of other schemes maps to a "domain" without
         // collisions. Given the |IsOriginSecure| check, just above, HTTP is
         // effectively restricted to just "localhost".
         (caller_origin.scheme() == url::kHttpScheme ||
          caller_origin.scheme() == url::kHttpsScheme);
}

// Ensure the relying party ID is a registrable domain suffix of or equal
// to the origin's effective domain. Reference:
// https://html.spec.whatwg.org/multipage/origin.html#is-a-registrable-domain-suffix-of-or-is-equal-to.
bool IsRelyingPartyIdValid(const std::string& relying_party_id,
                           url::Origin caller_origin) {
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

bool IsAppIdAllowedForOrigin(const GURL& appid, const url::Origin& origin) {
  // See
  // https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-appid-and-facets-v1.2-ps-20170411.html#determining-if-a-caller-s-facetid-is-authorized-for-an-appid

  // Step 1: "If the AppID is not an HTTPS URL, and matches the FacetID of the
  // caller, no additional processing is necessary and the operation may
  // proceed."

  // Webauthn is only supported on secure origins and |HasValidEffectiveDomain|
  // has already checked this property of |origin| before this call. Thus this
  // step is moot.
  DCHECK(content::IsOriginSecure(origin.GetURL()));

  // Step 2: "If the AppID is null or empty, the client must set the AppID to be
  // the FacetID of the caller, and the operation may proceed without additional
  // processing."

  // This step is handled before calling this function.

  // Step 3: "If the caller's FacetID is an https:// Origin sharing the same
  // host as the AppID, (e.g. if an application hosted at
  // https://fido.example.com/myApp set an AppID of
  // https://fido.example.com/myAppId), no additional processing is necessary
  // and the operation may proceed."
  if (origin.scheme() != url::kHttpsScheme ||
      appid.scheme_piece() != origin.scheme()) {
    return false;
  }

  // This check is repeated inside |SameDomainOrHost|, just after this. However
  // it's cheap and mirrors the structure of the spec.
  if (appid.host_piece() == origin.host()) {
    return true;
  }

  // At this point we diverge from the specification in order to avoid the
  // complexity of making a network request which isn't believed to be
  // neccessary in practice. See also
  // https://bugzilla.mozilla.org/show_bug.cgi?id=1244959#c8
  if (net::registry_controlled_domains::SameDomainOrHost(
          appid, origin,
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    return true;
  }

  // As a compatibility hack, sites within google.com are allowed to assert two
  // special-case AppIDs. Firefox also does this:
  // https://groups.google.com/forum/#!msg/mozilla.dev.platform/Uiu3fwnA2xw/201ynAiPAQAJ
  const GURL kGstatic1 =
      GURL("https://www.gstatic.com/securitykey/origins.json");
  const GURL kGstatic2 =
      GURL("https://www.gstatic.com/securitykey/a/google.com/origins.json");
  DCHECK(kGstatic1.is_valid() && kGstatic2.is_valid());

  if (origin.DomainIs("google.com") && !appid.has_ref() &&
      (appid.EqualsIgnoringRef(kGstatic1) ||
       appid.EqualsIgnoringRef(kGstatic2))) {
    return true;
  }

  return false;
}

device::CtapMakeCredentialRequest CreateCtapMakeCredentialRequest(
    base::span<const uint8_t, device::kClientDataHashLength> client_data_hash,
    const blink::mojom::PublicKeyCredentialCreationOptionsPtr& options,
    bool is_individual_attestation) {
  auto credential_params = mojo::ConvertTo<
      std::vector<device::PublicKeyCredentialParams::CredentialInfo>>(
      options->public_key_parameters);

  device::CtapMakeCredentialRequest make_credential_param(
      client_data_hash,
      mojo::ConvertTo<device::PublicKeyCredentialRpEntity>(
          options->relying_party),
      mojo::ConvertTo<device::PublicKeyCredentialUserEntity>(options->user),
      device::PublicKeyCredentialParams(std::move(credential_params)));

  auto exclude_list =
      mojo::ConvertTo<std::vector<device::PublicKeyCredentialDescriptor>>(
          options->exclude_credentials);

  make_credential_param.SetExcludeList(std::move(exclude_list));
  make_credential_param.SetIsIndividualAttestation(is_individual_attestation);
  make_credential_param.SetHmacSecret(options->hmac_create_secret);
  return make_credential_param;
}

device::CtapGetAssertionRequest CreateCtapGetAssertionRequest(
    base::span<const uint8_t, device::kClientDataHashLength> client_data_hash,
    const blink::mojom::PublicKeyCredentialRequestOptionsPtr& options,
    base::Optional<base::span<const uint8_t, device::kRpIdHashLength>>
        alternative_application_parameter) {
  device::CtapGetAssertionRequest request_parameter(options->relying_party_id,
                                                    client_data_hash);

  auto allowed_list =
      mojo::ConvertTo<std::vector<device::PublicKeyCredentialDescriptor>>(
          options->allow_credentials);

  request_parameter.SetAllowList(std::move(allowed_list));
  request_parameter.SetUserVerification(
      mojo::ConvertTo<device::UserVerificationRequirement>(
          options->user_verification));

  if (alternative_application_parameter) {
    request_parameter.SetAlternativeApplicationParameter(
        std::move(*alternative_application_parameter));
  }

  if (!options->cable_authentication_data.empty()) {
    request_parameter.SetCableExtension(
        mojo::ConvertTo<std::vector<device::CableDiscoveryData>>(
            options->cable_authentication_data));
  }
  return request_parameter;
}

std::array<uint8_t, crypto::kSHA256Length> ConstructClientDataHash(
    const std::string& client_data) {
  // SHA-256 hash of the JSON data structure.
  std::array<uint8_t, crypto::kSHA256Length> client_data_hash;
  crypto::SHA256HashString(client_data, client_data_hash.data(),
                           client_data_hash.size());
  return client_data_hash;
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

base::Optional<std::array<uint8_t, crypto::kSHA256Length>>
ProcessAppIdExtension(std::string appid, const url::Origin& caller_origin) {
  if (appid.empty()) {
    // See step two in the comments in |IsAppIdAllowedForOrigin|.
    appid = caller_origin.Serialize() + "/";
  }

  GURL appid_url = GURL(appid);
  if (!(appid_url.is_valid() &&
        IsAppIdAllowedForOrigin(appid_url, caller_origin))) {
    return base::nullopt;
  }

  return CreateApplicationParameter(appid);
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
        !base::ContainsValue(*out_transports, mapping.transport)) {
      out_transports->push_back(mapping.transport);
    }
  }
}

blink::mojom::MakeCredentialAuthenticatorResponsePtr
CreateMakeCredentialResponse(
    const std::string& client_data_json,
    device::AuthenticatorMakeCredentialResponse response_data,
    bool preserve_attestation) {
  auto response = blink::mojom::MakeCredentialAuthenticatorResponse::New();
  auto common_info = blink::mojom::CommonCredentialInfo::New();
  common_info->client_data_json.assign(client_data_json.begin(),
                                       client_data_json.end());
  common_info->raw_id = response_data.raw_credential_id();
  common_info->id = response_data.GetId();
  response->info = std::move(common_info);

  // The transport list must not contain duplicates but the order doesn't matter
  // because Blink will sort the resulting strings before returning them.
  std::vector<device::FidoTransportProtocol> transports = {
      response_data.transport_used()};
  // If the attestation certificate specifies that the token supports any other
  // transports, include them in the list.
  base::Optional<base::span<const uint8_t>> leaf_cert =
      response_data.attestation_object()
          .attestation_statement()
          .GetLeafCertificate();
  if (leaf_cert) {
    AppendUniqueTransportsFromCertificate(*leaf_cert, &transports);
  }

  for (auto transport : transports) {
    response->transports.push_back(
        mojo::ConvertTo<blink::mojom::AuthenticatorTransport>(transport));
  }

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

  if (!preserve_attestation) {
    response_data.EraseAttestationStatement();
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
      ? response->user_handle.emplace(response_data.user_entity()->user_id())
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

#if defined(OS_WIN)
  if (base::FeatureList::IsEnabled(features::kWebAuthCable) &&
      base::FeatureList::IsEnabled(features::kWebAuthCableWin)) {
#else
  if (base::FeatureList::IsEnabled(features::kWebAuthCable)) {
#endif  // defined(OS_WIN)
    transports.insert(
        device::FidoTransportProtocol::kCloudAssistedBluetoothLowEnergy);
  }
  return transports;
}

}  // namespace

AuthenticatorImpl::AuthenticatorImpl(RenderFrameHost* render_frame_host)
    : AuthenticatorImpl(render_frame_host,
                        nullptr /* connector */,
                        std::make_unique<base::OneShotTimer>()) {}

AuthenticatorImpl::AuthenticatorImpl(RenderFrameHost* render_frame_host,
                                     service_manager::Connector* connector,
                                     std::unique_ptr<base::OneShotTimer> timer)
    : WebContentsObserver(WebContents::FromRenderFrameHost(render_frame_host)),
      render_frame_host_(render_frame_host),
      connector_(connector),
      transports_(GetTransportsEnabledByFlags()),
      timer_(std::move(timer)),
      binding_(this),
      weak_factory_(this) {
  DCHECK(render_frame_host_);
  DCHECK(timer_);
}

AuthenticatorImpl::~AuthenticatorImpl() {
  // This call exists to assert that |render_frame_host_| outlives this object.
  // If this is violated, ASAN should notice.
  render_frame_host_->GetRoutingID();
}

void AuthenticatorImpl::Bind(blink::mojom::AuthenticatorRequest request) {
  // If |render_frame_host_| is being unloaded then binding requests are
  // rejected.
  if (!render_frame_host_->IsCurrent()) {
    return;
  }

  DCHECK(!binding_.is_bound());
  binding_.Bind(std::move(request));
}

void AuthenticatorImpl::UpdateRequestDelegate() {
  DCHECK(!request_delegate_);
  request_delegate_ =
      GetContentClient()->browser()->GetWebAuthenticationRequestDelegate(
          render_frame_host_);
}

bool AuthenticatorImpl::IsFocused() const {
  return render_frame_host_->IsCurrent() && request_delegate_->IsFocused();
}

// static
std::string AuthenticatorImpl::SerializeCollectedClientDataToJson(
    const std::string& type,
    const url::Origin& origin,
    base::span<const uint8_t> challenge) {
  static constexpr char kTypeKey[] = "type";
  static constexpr char kChallengeKey[] = "challenge";
  static constexpr char kOriginKey[] = "origin";

  base::DictionaryValue client_data;
  client_data.SetKey(kTypeKey, base::Value(type));
  client_data.SetKey(kChallengeKey, base::Value(Base64UrlEncode(challenge)));
  client_data.SetKey(kOriginKey, base::Value(origin.Serialize()));

  if (base::RandDouble() < 0.2) {
    // An extra key is sometimes added to ensure that RPs do not make
    // unreasonably specific assumptions about the clientData JSON. This is
    // done in the fashion of
    // https://tools.ietf.org/html/draft-davidben-tls-grease-01
    client_data.SetKey("new_keys_may_be_added_here",
                       base::Value("do not compare clientDataJSON against a "
                                   "template. See https://goo.gl/yabPex"));
  }

  std::string json;
  base::JSONWriter::Write(client_data, &json);
  return json;
}

// mojom::Authenticator
void AuthenticatorImpl::MakeCredential(
    blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
    MakeCredentialCallback callback) {
  if (request_) {
    std::move(callback).Run(blink::mojom::AuthenticatorStatus::PENDING_REQUEST,
                            nullptr);
    return;
  }

  UpdateRequestDelegate();
  if (!request_delegate_) {
    InvokeCallbackAndCleanup(std::move(callback),
                             blink::mojom::AuthenticatorStatus::PENDING_REQUEST,
                             nullptr, Focus::kDontCheck);
    return;
  }

  if (!IsFocused()) {
    InvokeCallbackAndCleanup(std::move(callback),
                             blink::mojom::AuthenticatorStatus::NOT_FOCUSED,
                             nullptr, Focus::kDontCheck);
    return;
  }

  url::Origin caller_origin = render_frame_host_->GetLastCommittedOrigin();
  relying_party_id_ = options->relying_party->id;

  if (!HasValidEffectiveDomain(caller_origin)) {
    bad_message::ReceivedBadMessage(render_frame_host_->GetProcess(),
                                    bad_message::AUTH_INVALID_EFFECTIVE_DOMAIN);
    InvokeCallbackAndCleanup(std::move(callback),
                             blink::mojom::AuthenticatorStatus::INVALID_DOMAIN,
                             nullptr, Focus::kDontCheck);
    return;
  }

  if (!IsRelyingPartyIdValid(relying_party_id_, caller_origin)) {
    bad_message::ReceivedBadMessage(render_frame_host_->GetProcess(),
                                    bad_message::AUTH_INVALID_RELYING_PARTY);
    InvokeCallbackAndCleanup(std::move(callback),
                             blink::mojom::AuthenticatorStatus::INVALID_DOMAIN,
                             nullptr, Focus::kDontCheck);
    return;
  }

  if (options->authenticator_selection &&
      options->authenticator_selection->require_resident_key) {
    // Disallow the creation of resident credentials.
    InvokeCallbackAndCleanup(
        std::move(callback),
        blink::mojom::AuthenticatorStatus::RESIDENT_CREDENTIALS_UNSUPPORTED,
        nullptr, Focus::kDontCheck);
    return;
  }

  DCHECK(make_credential_response_callback_.is_null());
  make_credential_response_callback_ = std::move(callback);

  timer_->Start(
      FROM_HERE, options->adjusted_timeout,
      base::BindOnce(&AuthenticatorImpl::OnTimeout, base::Unretained(this)));
  if (!connector_)
    connector_ = ServiceManagerConnection::GetForProcess()->GetConnector();

  // Save client data to return with the authenticator response.
  // TODO(kpaulhamus): Fetch and add the Token Binding ID public key used to
  // communicate with the origin.
  client_data_json_ = SerializeCollectedClientDataToJson(
      client_data::kCreateType, caller_origin, std::move(options->challenge));

  const bool individual_attestation =
      options->attestation ==
          blink::mojom::AttestationConveyancePreference::ENTERPRISE &&
      request_delegate_->ShouldPermitIndividualAttestation(relying_party_id_);

  attestation_preference_ = options->attestation;

  auto authenticator_selection_criteria =
      options->authenticator_selection
          ? mojo::ConvertTo<device::AuthenticatorSelectionCriteria>(
                options->authenticator_selection)
          : device::AuthenticatorSelectionCriteria();

  request_ = std::make_unique<device::MakeCredentialRequestHandler>(
      connector_, transports_,
      CreateCtapMakeCredentialRequest(
          ConstructClientDataHash(client_data_json_), options,
          individual_attestation),
      std::move(authenticator_selection_criteria),
      base::BindOnce(&AuthenticatorImpl::OnRegisterResponse,
                     weak_factory_.GetWeakPtr()));

  request_delegate_->RegisterActionCallbacks(
      base::BindOnce(&AuthenticatorImpl::Cancel,
                     weak_factory_.GetWeakPtr()) /* cancel_callback */,
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

  request_->SetPlatformAuthenticatorOrMarkUnavailable(
      CreatePlatformAuthenticatorIfAvailable());
}

// mojom:Authenticator
void AuthenticatorImpl::GetAssertion(
    blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
    GetAssertionCallback callback) {
  if (request_) {
    std::move(callback).Run(blink::mojom::AuthenticatorStatus::PENDING_REQUEST,
                            nullptr);
    return;
  }

  UpdateRequestDelegate();
  if (!request_delegate_) {
    InvokeCallbackAndCleanup(std::move(callback),
                             blink::mojom::AuthenticatorStatus::PENDING_REQUEST,
                             nullptr);
    return;
  }

  url::Origin caller_origin = render_frame_host_->GetLastCommittedOrigin();

  if (!HasValidEffectiveDomain(caller_origin)) {
    bad_message::ReceivedBadMessage(render_frame_host_->GetProcess(),
                                    bad_message::AUTH_INVALID_EFFECTIVE_DOMAIN);
    InvokeCallbackAndCleanup(std::move(callback),
                             blink::mojom::AuthenticatorStatus::INVALID_DOMAIN,
                             nullptr);
    return;
  }

  if (!IsRelyingPartyIdValid(options->relying_party_id, caller_origin)) {
    bad_message::ReceivedBadMessage(render_frame_host_->GetProcess(),
                                    bad_message::AUTH_INVALID_RELYING_PARTY);
    InvokeCallbackAndCleanup(std::move(callback),
                             blink::mojom::AuthenticatorStatus::INVALID_DOMAIN,
                             nullptr);
    return;
  }

  if (options->allow_credentials.empty()) {
    // Chrome currently does not support any resident keys.
    InvokeCallbackAndCleanup(
        std::move(callback),
        blink::mojom::AuthenticatorStatus::RESIDENT_CREDENTIALS_UNSUPPORTED,
        nullptr);
    return;
  }

  if (options->appid) {
    alternative_application_parameter_ =
        ProcessAppIdExtension(*options->appid, caller_origin);
    if (!alternative_application_parameter_) {
      std::move(callback).Run(blink::mojom::AuthenticatorStatus::INVALID_DOMAIN,
                              nullptr);
      return;
    }
  }

  DCHECK(get_assertion_response_callback_.is_null());
  get_assertion_response_callback_ = std::move(callback);

  timer_->Start(
      FROM_HERE, options->adjusted_timeout,
      base::BindOnce(&AuthenticatorImpl::OnTimeout, base::Unretained(this)));

  if (!connector_)
    connector_ = ServiceManagerConnection::GetForProcess()->GetConnector();

  // Save client data to return with the authenticator response.
  // TODO(kpaulhamus): Fetch and add the Token Binding ID public key used to
  // communicate with the origin.
  client_data_json_ = SerializeCollectedClientDataToJson(
      client_data::kGetType, caller_origin, std::move(options->challenge));

  auto ctap_request = CreateCtapGetAssertionRequest(
      ConstructClientDataHash(client_data_json_), std::move(options),
      alternative_application_parameter_);

  auto opt_platform_authenticator_info =
      CreatePlatformAuthenticatorIfAvailableAndCheckIfCredentialExists(
          ctap_request);
  request_ = std::make_unique<device::GetAssertionRequestHandler>(
      connector_, transports_, std::move(ctap_request),
      base::BindOnce(&AuthenticatorImpl::OnSignResponse,
                     weak_factory_.GetWeakPtr()));

  request_delegate_->RegisterActionCallbacks(
      base::BindOnce(&AuthenticatorImpl::Cancel,
                     weak_factory_.GetWeakPtr()) /* cancel_callback */,
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

  request_->SetPlatformAuthenticatorOrMarkUnavailable(
      std::move(opt_platform_authenticator_info));
}

void AuthenticatorImpl::IsUserVerifyingPlatformAuthenticatorAvailable(
    IsUserVerifyingPlatformAuthenticatorAvailableCallback callback) {
  const bool result = IsUserVerifyingPlatformAuthenticatorAvailableImpl();
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), result));
}

bool AuthenticatorImpl::IsUserVerifyingPlatformAuthenticatorAvailableImpl() {
#if defined(OS_MACOSX)
  // Touch ID is disabled, regardless of hardware support, if the embedder
  // doesn't support it or if this is an Incognito session. N.B.
  // request_delegate_ may be nullptr at this point.
  if (!GetContentClient()
           ->browser()
           ->IsWebAuthenticationTouchIdAuthenticatorSupported() ||
      browser_context()->IsOffTheRecord()) {
    return false;
  }

  return device::fido::mac::TouchIdAuthenticator::IsAvailable();
#else
  return false;
#endif
}

void AuthenticatorImpl::DidFinishNavigation(
    NavigationHandle* navigation_handle) {
  // If the RenderFrameHost itself is navigated then this function will cause
  // request state to be cleaned up. It's also possible for a navigation in the
  // same frame to use a fresh RenderFrameHost. In this case,
  // |render_frame_host_->IsCurrent()| will start returning false, causing all
  // focus checks to fail if any Mojo requests are made in that state.
  if (!navigation_handle->HasCommitted() ||
      navigation_handle->IsSameDocument() ||
      navigation_handle->GetRenderFrameHost() != render_frame_host_) {
    return;
  }

  binding_.Close();
  Cleanup();
}

// Callback to handle the async registration response from a U2fDevice.
void AuthenticatorImpl::OnRegisterResponse(
    device::FidoReturnCode status_code,
    base::Optional<device::AuthenticatorMakeCredentialResponse> response_data,
    device::FidoTransportProtocol transport_used) {
  if (!request_) {
    // Either the callback was called immediately and |request_| has not yet
    // been assigned (this is a bug), or a navigation caused the request to be
    // canceled while a callback was enqueued.
    return;
  }

  switch (status_code) {
    case device::FidoReturnCode::kUserConsentButCredentialExcluded:
      // Duplicate registration: the new credential would be created on an
      // authenticator that already contains one of the credentials in
      // |exclude_credentials|.
      DCHECK(request_delegate_);
      request_delegate_->DidFailWithInterestingReason(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kKeyAlreadyRegistered);
      InvokeCallbackAndCleanup(
          std::move(make_credential_response_callback_),
          blink::mojom::AuthenticatorStatus::CREDENTIAL_EXCLUDED, nullptr,
          Focus::kDoCheck);
      return;
    case device::FidoReturnCode::kAuthenticatorResponseInvalid:
      // The response from the authenticator was corrupted.
      InvokeCallbackAndCleanup(
          std::move(make_credential_response_callback_),
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR, nullptr,
          Focus::kDoCheck);
      return;
    case device::FidoReturnCode::kUserConsentButCredentialNotRecognized:
      // TODO(crbug/876109): This isn't strictly unreachable.
      NOTREACHED();
      return;
    case device::FidoReturnCode::kUserConsentDenied:
      InvokeCallbackAndCleanup(
          std::move(make_credential_response_callback_),
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR, nullptr,
          Focus::kDoCheck);
      return;
    case device::FidoReturnCode::kSuccess:
      DCHECK(response_data.has_value());
      request_delegate_->UpdateLastTransportUsed(transport_used);

      if (attestation_preference_ !=
          blink::mojom::AttestationConveyancePreference::NONE) {
        UMA_HISTOGRAM_ENUMERATION("WebAuthentication.AttestationPromptResult",
                                  AttestationPromptResult::kQueried);
        awaiting_attestation_response_ = true;
        request_delegate_->ShouldReturnAttestation(
            relying_party_id_,
            base::BindOnce(
                &AuthenticatorImpl::OnRegisterResponseAttestationDecided,
                weak_factory_.GetWeakPtr(), std::move(*response_data)));
        return;
      }

      const bool include_attestation = response_data->IsSelfAttestation();
      InvokeCallbackAndCleanup(
          std::move(make_credential_response_callback_),
          blink::mojom::AuthenticatorStatus::SUCCESS,
          CreateMakeCredentialResponse(std::move(client_data_json_),
                                       std::move(*response_data),
                                       include_attestation),
          Focus::kDoCheck);
      return;
  }
  NOTREACHED();
}

void AuthenticatorImpl::OnRegisterResponseAttestationDecided(
    device::AuthenticatorMakeCredentialResponse response_data,
    bool attestation_permitted) {
  awaiting_attestation_response_ = false;
  if (!request_) {
    // The request has already been cleaned up, probably because a navigation
    // occured while the permissions prompt was pending.
    return;
  }

  DCHECK(attestation_preference_ !=
         blink::mojom::AttestationConveyancePreference::NONE);

  if (!attestation_permitted) {
    UMA_HISTOGRAM_ENUMERATION("WebAuthentication.AttestationPromptResult",
                              AttestationPromptResult::kBlocked);
    InvokeCallbackAndCleanup(
        std::move(make_credential_response_callback_),
        blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR, nullptr,
        Focus::kDoCheck);
    return;
  }

  UMA_HISTOGRAM_ENUMERATION("WebAuthentication.AttestationPromptResult",
                            AttestationPromptResult::kAllowed);
  bool include_attestation = true;

  // The check for IsAttestationCertificateInappropriatelyIdentifying is
  // performed after the permissions prompt, even though we know the answer
  // before, because this still effectively discloses the make & model of the
  // authenticator: If an RP sees a "none" attestation from Chrome after
  // requesting direct attestation then it knows that it was one of the tokens
  // with inappropriate certs.
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
    include_attestation = false;
  }

  InvokeCallbackAndCleanup(std::move(make_credential_response_callback_),
                           blink::mojom::AuthenticatorStatus::SUCCESS,
                           CreateMakeCredentialResponse(
                               std::move(client_data_json_),
                               std::move(response_data), include_attestation),
                           Focus::kDoCheck);
}

void AuthenticatorImpl::OnSignResponse(
    device::FidoReturnCode status_code,
    base::Optional<device::AuthenticatorGetAssertionResponse> response_data,
    device::FidoTransportProtocol transport_used) {
  if (!request_) {
    // Either the callback was called immediately and |request_| has not yet
    // been assigned (this is a bug), or a navigation caused the request to be
    // canceled while a callback was enqueued.
    return;
  }

  switch (status_code) {
    case device::FidoReturnCode::kUserConsentButCredentialNotRecognized:
      // No authenticators contained the credential.
      DCHECK(request_delegate_);
      request_delegate_->DidFailWithInterestingReason(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kKeyNotRegistered);
      InvokeCallbackAndCleanup(
          std::move(get_assertion_response_callback_),
          blink::mojom::AuthenticatorStatus::CREDENTIAL_NOT_RECOGNIZED,
          nullptr);
      return;
    case device::FidoReturnCode::kAuthenticatorResponseInvalid:
      // The response from the authenticator was corrupted.
      InvokeCallbackAndCleanup(
          std::move(get_assertion_response_callback_),
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR, nullptr);
      return;
    case device::FidoReturnCode::kUserConsentButCredentialExcluded:
      // TODO(crbug/876109): This isn't strictly unreachable.
      NOTREACHED();
      return;
    case device::FidoReturnCode::kUserConsentDenied:
      InvokeCallbackAndCleanup(
          std::move(get_assertion_response_callback_),
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR, nullptr);
      return;
    case device::FidoReturnCode::kSuccess:
      DCHECK(response_data.has_value());
      request_delegate_->UpdateLastTransportUsed(transport_used);

      base::Optional<bool> echo_appid_extension;
      if (alternative_application_parameter_) {
        echo_appid_extension = (response_data->GetRpIdHash() ==
                                *alternative_application_parameter_);
      }
      InvokeCallbackAndCleanup(
          std::move(get_assertion_response_callback_),
          blink::mojom::AuthenticatorStatus::SUCCESS,
          CreateGetAssertionResponse(std::move(client_data_json_),
                                     std::move(*response_data),
                                     echo_appid_extension));
      return;
  }
  NOTREACHED();
}

void AuthenticatorImpl::FailWithNotAllowedErrorAndCleanup() {
  DCHECK(make_credential_response_callback_ ||
         get_assertion_response_callback_);
  if (make_credential_response_callback_) {
    InvokeCallbackAndCleanup(
        std::move(make_credential_response_callback_),
        blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR, nullptr,
        Focus::kDontCheck);
  } else if (get_assertion_response_callback_) {
    InvokeCallbackAndCleanup(
        std::move(get_assertion_response_callback_),
        blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR, nullptr);
  }
}

void AuthenticatorImpl::OnTimeout() {
  DCHECK(request_delegate_);
  if (awaiting_attestation_response_) {
    UMA_HISTOGRAM_ENUMERATION("WebAuthentication.AttestationPromptResult",
                              AttestationPromptResult::kTimeout);
    awaiting_attestation_response_ = false;
  }

  request_delegate_->DidFailWithInterestingReason(
      AuthenticatorRequestClientDelegate::InterestingFailureReason::kTimeout);

  // TODO(crbug.com/814418): Add layout tests to verify timeouts are
  // indistinguishable from NOT_ALLOWED_ERROR cases.
  FailWithNotAllowedErrorAndCleanup();
}

void AuthenticatorImpl::Cancel() {
  // If response callback is invoked already, then ignore cancel request.
  if (!make_credential_response_callback_ && !get_assertion_response_callback_)
    return;

  FailWithNotAllowedErrorAndCleanup();
}

void AuthenticatorImpl::InvokeCallbackAndCleanup(
    MakeCredentialCallback callback,
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

void AuthenticatorImpl::InvokeCallbackAndCleanup(
    GetAssertionCallback callback,
    blink::mojom::AuthenticatorStatus status,
    blink::mojom::GetAssertionAuthenticatorResponsePtr response) {
  std::move(callback).Run(status, std::move(response));
  Cleanup();
}

void AuthenticatorImpl::Cleanup() {
  if (awaiting_attestation_response_) {
    UMA_HISTOGRAM_ENUMERATION("WebAuthentication.AttestationPromptResult",
                              AttestationPromptResult::kAbandoned);
    awaiting_attestation_response_ = false;
  }

  timer_->Stop();
  request_.reset();
  request_delegate_.reset();
  make_credential_response_callback_.Reset();
  get_assertion_response_callback_.Reset();
  client_data_json_.clear();
  alternative_application_parameter_.reset();
}

BrowserContext* AuthenticatorImpl::browser_context() const {
  return content::WebContents::FromRenderFrameHost(render_frame_host_)
      ->GetBrowserContext();
}

#if defined(OS_MACOSX)
namespace {
std::unique_ptr<device::fido::mac::TouchIdAuthenticator>
CreateTouchIdAuthenticatorIfAvailable(
    const AuthenticatorRequestClientDelegate* request_delegate) {
  // Not all embedders may provide an authenticator config.
  auto opt_authenticator_config =
      request_delegate->GetTouchIdAuthenticatorConfig();
  if (!opt_authenticator_config) {
    return nullptr;
  }
  return device::fido::mac::TouchIdAuthenticator::CreateIfAvailable(
      std::move(opt_authenticator_config->keychain_access_group),
      std::move(opt_authenticator_config->metadata_secret));
}
}  // namespace
#endif

base::Optional<device::PlatformAuthenticatorInfo>
AuthenticatorImpl::CreatePlatformAuthenticatorIfAvailable() {
  // Incognito mode disables platform authenticators, so check for availability
  // first.
  if (!IsUserVerifyingPlatformAuthenticatorAvailableImpl()) {
    return base::nullopt;
  }
#if defined(OS_MACOSX)
  return device::PlatformAuthenticatorInfo(
      CreateTouchIdAuthenticatorIfAvailable(request_delegate_.get()), false);
#else
  return base::nullopt;
#endif
}

base::Optional<device::PlatformAuthenticatorInfo> AuthenticatorImpl::
    CreatePlatformAuthenticatorIfAvailableAndCheckIfCredentialExists(
        const device::CtapGetAssertionRequest& request) {
  // Incognito mode disables platform authenticators, so check for availability
  // first.
  if (!IsUserVerifyingPlatformAuthenticatorAvailableImpl()) {
    return base::nullopt;
  }
#if defined(OS_MACOSX)
  std::unique_ptr<device::fido::mac::TouchIdAuthenticator> authenticator =
      CreateTouchIdAuthenticatorIfAvailable(request_delegate_.get());
  const bool has_credential =
      authenticator->HasCredentialForGetAssertionRequest(request);
  return device::PlatformAuthenticatorInfo(std::move(authenticator),
                                           has_credential);
#else
  return base::nullopt;
#endif
}

}  // namespace content