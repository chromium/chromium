// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/authenticator_common_impl.h"

#include <array>
#include <string>
#include <utility>
#include <vector>

#include "base/base64url.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "base/strings/string_piece.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "content/browser/renderer_host/back_forward_cache_disable.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webauth/authenticator_environment.h"
#include "content/browser/webauth/client_data_json.h"
#include "content/browser/webauth/virtual_authenticator_manager_impl.h"
#include "content/browser/webauth/virtual_fido_discovery_factory.h"
#include "content/browser/webauth/webauth_request_security_checker.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "crypto/sha2.h"
#include "device/fido/attestation_statement.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/features.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/filter.h"
#include "device/fido/get_assertion_request_handler.h"
#include "device/fido/make_credential_request_handler.h"
#include "device/fido/public_key.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_params.h"
#include "net/cert/asn1_util.h"
#include "net/der/input.h"
#include "net/der/parse_values.h"
#include "net/der/parser.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_MAC)
#include "device/fido/mac/authenticator.h"
#include "device/fido/mac/credential_metadata.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "device/fido/cros/authenticator.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "device/fido/features.h"
#include "device/fido/win/authenticator.h"
#include "device/fido/win/webauthn_api.h"
#endif

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_WIN)
#include "content/browser/webauth/is_uvpaa.h"
#endif

namespace content {

// RequestExtension is a type of extension in a WebAuthn request that might
// yield an extension output in the response.
enum class RequestExtension {
  kAppID,
  kHMACSecret,
  kPRF,
  kCredProps,
  kLargeBlobEnable,
  kLargeBlobRead,
  kLargeBlobWrite,
  kCredBlob,
  kGetCredBlob,
  kMinPINLength,
  kDevicePublicKey,
};

enum class AttestationErasureOption {
  kIncludeAttestation,
  kEraseAttestationButIncludeAaguid,
  kEraseAttestationAndAaguid,
};

namespace {

WebAuthenticationDelegate* GetWebAuthenticationDelegate() {
  return GetContentClient()->browser()->GetWebAuthenticationDelegate();
}

std::string Base64UrlEncode(const base::span<const uint8_t> input) {
  std::string ret;
  base::Base64UrlEncode(
      base::StringPiece(reinterpret_cast<const char*>(input.data()),
                        input.size()),
      base::Base64UrlEncodePolicy::OMIT_PADDING, &ret);
  return ret;
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
    absl::optional<std::string> app_id) {
  device::CtapGetAssertionRequest request_parameter(options->relying_party_id,
                                                    client_data_json);

  request_parameter.allow_list = options->allow_credentials;

  request_parameter.user_verification = options->user_verification;

  if (app_id) {
    request_parameter.alternative_application_parameter =
        CreateApplicationParameter(*app_id);
    request_parameter.app_id = std::move(*app_id);
  }

  if (!options->extensions->cable_authentication_data.empty()) {
    request_parameter.cable_extension =
        options->extensions->cable_authentication_data;
  }
  return request_parameter;
}

// Parses the FIDO transport types extension from the DER-encoded, X.509
// certificate in |der_cert| and adds any transport types found to
// |out_transports|. Returns true if any transports were added.
bool AddTransportsFromCertificate(
    base::span<const uint8_t> der_cert,
    base::flat_set<device::FidoTransportProtocol>* out_transports) {
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
    return false;
  }

  const net::der::Input contents_der(contents);
  net::der::Parser contents_parser(contents_der);
  absl::optional<net::der::BitString> transport_bits =
      contents_parser.ReadBitString();
  if (!transport_bits) {
    return false;
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

  bool ret = false;
  for (const auto& mapping : kTransportMapping) {
    if (transport_bits->AssertsBit(mapping.bit_index)) {
      out_transports->insert(mapping.transport);
      ret |= true;
    }
  }

  return ret;
}

base::TimeDelta AdjustTimeout(absl::optional<base::TimeDelta> timeout,
                              RenderFrameHost* render_frame_host) {
  // Time to wait for an authenticator to successfully complete an operation.
  static constexpr base::TimeDelta kAdjustedTimeoutLower = base::Seconds(10);
  static constexpr base::TimeDelta kAdjustedTimeoutUpper = base::Minutes(10);

  if (!timeout) {
    return kAdjustedTimeoutUpper;
  }
  const bool testing_api_enabled =
      AuthenticatorEnvironment::GetInstance()->IsVirtualAuthenticatorEnabledFor(
          static_cast<RenderFrameHostImpl*>(render_frame_host)
              ->frame_tree_node());
  if (testing_api_enabled) {
    return *timeout;
  }
  return std::max(kAdjustedTimeoutLower,
                  std::min(kAdjustedTimeoutUpper, *timeout));
}

bool UsesDiscoverableCreds(const device::MakeCredentialOptions& options) {
  return options.resident_key == device::ResidentKeyRequirement::kRequired;
}

bool UsesDiscoverableCreds(const device::CtapGetAssertionRequest& request) {
  return request.allow_list.empty();
}

// GetWebAuthnTransports returns the set of transports that should be passed to
// a FidoRequestHandler for a WebAuthn request. This determines for which
// transports the request handler will attempt to obtain FidoDiscovery
// instances.
base::flat_set<device::FidoTransportProtocol> GetWebAuthnTransports(
    RenderFrameHost* render_frame_host,
    device::FidoDiscoveryFactory* discovery_factory,
    bool uses_discoverable_creds) {
  base::flat_set<device::FidoTransportProtocol> transports;
  transports.insert(device::FidoTransportProtocol::kUsbHumanInterfaceDevice);

  // Only instantiate platform discovery if the embedder hasn't chosen to
  // override IsUserVerifyingPlatformAuthenticatorAvailable() to be false.
  // Chrome disables platform authenticators in Guest modes this way.
  absl::optional<bool> embedder_isuvpaa_override =
      GetWebAuthenticationDelegate()
          ->IsUserVerifyingPlatformAuthenticatorAvailableOverride(
              render_frame_host);
  if (!embedder_isuvpaa_override || *embedder_isuvpaa_override) {
    transports.insert(device::FidoTransportProtocol::kInternal);
  }

  if (discovery_factory->IsTestOverride()) {
    // The desktop implementation does not support BLE or NFC, but we emulate
    // them if the testing API is enabled.
    transports.insert(device::FidoTransportProtocol::kBluetoothLowEnergy);
    transports.insert(device::FidoTransportProtocol::kNearFieldCommunication);

    // Ensure virtual platform authenticators can be instantiated even if they
    // are not-user-verifying, i.e. IsUVPAA() returns false.
    transports.insert(device::FidoTransportProtocol::kInternal);
  }

  transports.insert(device::FidoTransportProtocol::kHybrid);

  // kAndroidAccessory doesn't work on Windows because of USB stack issues.
  // Note: even if this value were inserted it wouldn't take effect on Windows
  // versions with a native API because FidoRequestHandlerBase filters out
  // non-kHybrid transports in that case.
#if !BUILDFLAG(IS_WIN)
  // In order for AOA to be active the |AuthenticatorRequestClientDelegate|
  // must still configure a |UsbDeviceManager|.
  transports.insert(device::FidoTransportProtocol::kAndroidAccessory);
#endif

  return transports;
}

// Returns a new FidoDiscoveryFactory for the current request. This may be
// a factory for virtual authenticators if the testing API is enabled for the
// given frame.
std::unique_ptr<device::FidoDiscoveryFactory> MakeDiscoveryFactory(
    RenderFrameHost* render_frame_host) {
  VirtualAuthenticatorManagerImpl* virtual_authenticator_manager =
      AuthenticatorEnvironment::GetInstance()
          ->MaybeGetVirtualAuthenticatorManager(
              static_cast<RenderFrameHostImpl*>(render_frame_host)
                  ->frame_tree_node());
  if (virtual_authenticator_manager) {
    return virtual_authenticator_manager->MakeDiscoveryFactory();
  }

  auto discovery_factory = std::make_unique<device::FidoDiscoveryFactory>();

#if BUILDFLAG(IS_MAC)
  discovery_factory->set_mac_touch_id_info(
      GetWebAuthenticationDelegate()->GetTouchIdAuthenticatorConfig(
          render_frame_host->GetBrowserContext()));
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_CHROMEOS)
  // Ignore the ChromeOS u2fd virtual U2F HID device so that it doesn't collide
  // with the ChromeOS platform authenticator, also implemented in u2fd.
  if (base::FeatureList::IsEnabled(device::kWebAuthCrosPlatformAuthenticator)) {
    // There are two possible PIDs the virtual U2F HID device could use, with or
    // without corp protocol functionality.
    constexpr device::VidPid kChromeOsU2fdVidPid{0x18d1, 0x502c};
    constexpr device::VidPid kChromeOsU2fdCorpVidPid{0x18d1, 0x5212};
    discovery_factory->set_hid_ignore_list(
        {kChromeOsU2fdVidPid, kChromeOsU2fdCorpVidPid});
    discovery_factory->set_generate_request_id_callback(
        GetWebAuthenticationDelegate()->GetGenerateRequestIdCallback(
            render_frame_host));
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  return discovery_factory;
}

absl::optional<device::CredProtectRequest> ProtectionPolicyToCredProtect(
    blink::mojom::ProtectionPolicy protection_policy,
    const device::MakeCredentialOptions& make_credential_options) {
  switch (protection_policy) {
    case blink::mojom::ProtectionPolicy::UNSPECIFIED:
      // Some platform authenticators have the behaviour that uv=required
      // demands a local reauthentication but uv=preferred can be satisfied by
      // just clicking a button. Since the device has to be unlocked by the
      // user, this seems to balance the demands of uv=required against the
      // fact that quite a number of (non-mobile) devices lack biometrics and
      // thus full UV requires entering the local password. Since password
      // autofill doesn't demand entering the local password all the time, it
      // would be sad if WebAuthn was much worse in that respect.
      //
      // Also, some sites have (or will) implement a sign-in flow where the
      // user enters their username and then the site makes a WebAuthn
      // request, with an allowlist, where completing that request is
      // sufficient to sign-in. I.e. there's no additional password challenge.
      // Since these sites are trying to replace passwords, we expect them to
      // set uv=preferred in order to work well with the platform behaviour
      // detailed in the first paragraph.
      //
      // If such sites remembered the UV flag from the registration and enforced
      // it at assertion time, that would break situations where closing a
      // laptop lid covers the biometric sensor and makes entering a password
      // preferable. But without any enforcement of the UV flag, someone could
      // pick a security key off the ground and do a uv=false request to get a
      // sufficient assertion.
      //
      // Thus if rk=required and uv=preferred, credProtect level three is set
      // to tell security keys to only create an assertion after UV for this
      // credential. (Sites can still override this by setting a specific
      // credProtect level.)
      //
      // If a site sets rk=preferred then we assume that they're doing something
      // unusual and will only set credProtect level two.
      //
      // See also
      // https://chromium.googlesource.com/chromium/src/+/main/content/browser/webauth/cred_protect.md
      if (make_credential_options.resident_key ==
              device::ResidentKeyRequirement::kRequired &&
          make_credential_options.user_verification ==
              device::UserVerificationRequirement::kPreferred) {
        return device::CredProtectRequest::kUVRequired;
      }
      if (make_credential_options.resident_key !=
          device::ResidentKeyRequirement::kDiscouraged) {
        // Otherwise, kUVOrCredIDRequired is made the default unless
        // the authenticator defaults to something better.
        return device::CredProtectRequest::kUVOrCredIDRequiredOrBetter;
      }
      return absl::nullopt;
    case blink::mojom::ProtectionPolicy::NONE:
      return device::CredProtectRequest::kUVOptional;
    case blink::mojom::ProtectionPolicy::UV_OR_CRED_ID_REQUIRED:
      return device::CredProtectRequest::kUVOrCredIDRequired;
    case blink::mojom::ProtectionPolicy::UV_REQUIRED:
      return device::CredProtectRequest::kUVRequired;
  }
}

}  // namespace

// static
std::unique_ptr<AuthenticatorCommon> AuthenticatorCommon::Create(
    RenderFrameHost* render_frame_host) {
  return std::make_unique<AuthenticatorCommonImpl>(render_frame_host);
}

AuthenticatorCommonImpl::AuthenticatorCommonImpl(
    RenderFrameHost* render_frame_host)
    : render_frame_host_id_(render_frame_host->GetGlobalId()),
      security_checker_(static_cast<RenderFrameHostImpl*>(render_frame_host)
                            ->GetWebAuthRequestSecurityChecker()) {
  // Disable the back-forward cache for any document that makes WebAuthn
  // requests. Pages using privacy-sensitive APIs are generally exempt from
  // back-forward cache for now as a precaution.
  BackForwardCache::DisableForRenderFrameHost(
      render_frame_host,
      BackForwardCacheDisable::DisabledReason(
          BackForwardCacheDisable::DisabledReasonId::kWebAuthenticationAPI));
}

AuthenticatorCommonImpl::~AuthenticatorCommonImpl() = default;

std::unique_ptr<AuthenticatorRequestClientDelegate>
AuthenticatorCommonImpl::MaybeCreateRequestDelegate() {
  RenderFrameHostImpl* const render_frame_host_impl =
      static_cast<RenderFrameHostImpl*>(GetRenderFrameHost());
  std::unique_ptr<AuthenticatorRequestClientDelegate> delegate =
      GetContentClient()->browser()->GetWebAuthenticationRequestDelegate(
          render_frame_host_impl);
  if (!delegate) {
    return nullptr;
  }
  VirtualAuthenticatorManagerImpl* virtual_authenticator_manager =
      AuthenticatorEnvironment::GetInstance()
          ->MaybeGetVirtualAuthenticatorManager(
              render_frame_host_impl->frame_tree_node());
  if (virtual_authenticator_manager) {
    delegate->SetVirtualEnvironment(true);
    if (!virtual_authenticator_manager->is_ui_enabled()) {
      delegate->DisableUI();
    }
  }
  return delegate;
}

void AuthenticatorCommonImpl::StartMakeCredentialRequest(
    bool allow_skipping_pin_touch) {
  InitDiscoveryFactory();

  request_delegate_->ConfigureCable(
      caller_origin_, device::FidoRequestType::kMakeCredential,
      make_credential_options_->resident_key,
      base::span<const device::CableDiscoveryData>(), discovery_factory());

  make_credential_options_->allow_skipping_pin_touch = allow_skipping_pin_touch;

  base::flat_set<device::FidoTransportProtocol> transports =
      GetWebAuthnTransports(GetRenderFrameHost(), discovery_factory(),
                            UsesDiscoverableCreds(*make_credential_options_));

  request_handler_ = std::make_unique<device::MakeCredentialRequestHandler>(
      discovery_factory(), transports, *ctap_make_credential_request_,
      *make_credential_options_,
      base::BindOnce(&AuthenticatorCommonImpl::OnRegisterResponse,
                     weak_factory_.GetWeakPtr()));

  request_delegate_->RegisterActionCallbacks(
      base::BindOnce(&AuthenticatorCommonImpl::OnCancelFromUI,
                     weak_factory_.GetWeakPtr()) /* cancel_callback */,
      base::BindRepeating(
          &AuthenticatorCommonImpl::StartMakeCredentialRequest,
          weak_factory_.GetWeakPtr(),
          /*allow_skipping_pin_touch=*/false) /* start_over_callback */,
      base::DoNothing() /* account_preselected_callback */,
      base::BindRepeating(
          &device::FidoRequestHandlerBase::StartAuthenticatorRequest,
          request_handler_->GetWeakPtr()) /* request_callback */,
      base::BindRepeating(
          &device::FidoRequestHandlerBase::PowerOnBluetoothAdapter,
          request_handler_
              ->GetWeakPtr()) /* bluetooth_adapter_power_on_callback */);
  request_handler_->set_observer(request_delegate_.get());
}

void AuthenticatorCommonImpl::StartGetAssertionRequest(
    bool allow_skipping_pin_touch) {
  InitDiscoveryFactory();

  base::span<const device::CableDiscoveryData> cable_pairings;
  if (ctap_get_assertion_request_->cable_extension && IsFocused()) {
    cable_pairings = *ctap_get_assertion_request_->cable_extension;
  }
  request_delegate_->ConfigureCable(caller_origin_,
                                    device::FidoRequestType::kGetAssertion,
                                    /*resident_key_requirement=*/absl::nullopt,
                                    cable_pairings, discovery_factory());
#if BUILDFLAG(IS_CHROMEOS)
  discovery_factory()->set_get_assertion_request_for_legacy_credential_check(
      *ctap_get_assertion_request_);
#endif

  base::flat_set<device::FidoTransportProtocol> transports =
      GetWebAuthnTransports(
          GetRenderFrameHost(), discovery_factory(),
          UsesDiscoverableCreds(*ctap_get_assertion_request_));

  auto request_handler = std::make_unique<device::GetAssertionRequestHandler>(
      discovery_factory(), transports, *ctap_get_assertion_request_,
      *ctap_get_assertion_options_, allow_skipping_pin_touch,
      base::BindOnce(&AuthenticatorCommonImpl::OnSignResponse,
                     weak_factory_.GetWeakPtr()));

  request_delegate_->RegisterActionCallbacks(
      base::BindOnce(&AuthenticatorCommonImpl::OnCancelFromUI,
                     weak_factory_.GetWeakPtr()) /* cancel_callback */,
      base::BindRepeating(
          &AuthenticatorCommonImpl::StartGetAssertionRequest,
          weak_factory_.GetWeakPtr(),
          /*allow_skipping_pin_touch=*/false) /* start_over_callback */,
      base::BindRepeating(
          &device::GetAssertionRequestHandler::PreselectAccount,
          request_handler->GetWeakPtr()) /* account_preselected_callback */,
      base::BindRepeating(
          &device::GetAssertionRequestHandler::StartAuthenticatorRequest,
          request_handler->GetWeakPtr()) /* request_callback */,
      base::BindRepeating(
          &device::FidoRequestHandlerBase::PowerOnBluetoothAdapter,
          request_handler
              ->GetWeakPtr()) /* bluetooth_adapter_power_on_callback */);

  request_handler->set_observer(request_delegate_.get());
  request_handler_ = std::move(request_handler);
}

bool AuthenticatorCommonImpl::IsFocused() const {
  return GetRenderFrameHost()->IsActive() &&
         GetWebAuthenticationDelegate()->IsFocused(
             WebContents::FromRenderFrameHost(GetRenderFrameHost()));
}

// mojom::Authenticator
void AuthenticatorCommonImpl::MakeCredential(
    url::Origin caller_origin,
    blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
    blink::mojom::Authenticator::MakeCredentialCallback callback) {
  if (has_pending_request_) {
    std::move(callback).Run(blink::mojom::AuthenticatorStatus::PENDING_REQUEST,
                            nullptr, nullptr);
    return;
  }
  has_pending_request_ = true;

  DCHECK(make_credential_response_callback_.is_null());
  make_credential_response_callback_ = std::move(callback);

  BeginRequestTimeout(options->timeout);

  WebAuthRequestSecurityChecker::RequestType request_type =
      options->is_payment_credential_creation
          ? WebAuthRequestSecurityChecker::RequestType::kMakePaymentCredential
          : WebAuthRequestSecurityChecker::RequestType::kMakeCredential;
  bool is_cross_origin_iframe = false;
  blink::mojom::AuthenticatorStatus status =
      security_checker_->ValidateAncestorOrigins(caller_origin, request_type,
                                                 &is_cross_origin_iframe);
  if (status != blink::mojom::AuthenticatorStatus::SUCCESS) {
    CompleteMakeCredentialRequest(status);
    return;
  }

  if (!security_checker_->DeduplicateCredentialDescriptorListAndValidateLength(
          &options->exclude_credentials)) {
    mojo::ReportBadMessage("invalid exclude_credentials length");
    CompleteMakeCredentialRequest(
        blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
    return;
  }

  status = security_checker_->ValidateDomainAndRelyingPartyID(
      caller_origin, options->relying_party.id, request_type,
      options->remote_desktop_client_override);
  if (status != blink::mojom::AuthenticatorStatus::SUCCESS) {
    CompleteMakeCredentialRequest(status);
    return;
  }

  request_delegate_ = MaybeCreateRequestDelegate();
  if (!request_delegate_) {
    CompleteMakeCredentialRequest(
        blink::mojom::AuthenticatorStatus::PENDING_REQUEST);
    return;
  }

  if (!request_delegate_->IsVirtualEnvironmentEnabled() &&
      !disable_tls_check_ &&
      !GetWebAuthenticationDelegate()->IsSecurityLevelAcceptableForWebAuthn(
          GetRenderFrameHost(), caller_origin)) {
    CompleteMakeCredentialRequest(
        blink::mojom::AuthenticatorStatus::CERTIFICATE_ERROR);
    return;
  }

  caller_origin_ = caller_origin;
  relying_party_id_ = options->relying_party.id;

  absl::optional<std::string> appid_exclude;
  if (options->appid_exclude) {
    appid_exclude = "";
    auto add_id_status = security_checker_->ValidateAppIdExtension(
        *options->appid_exclude, caller_origin,
        options->remote_desktop_client_override, &appid_exclude.value());
    if (add_id_status != blink::mojom::AuthenticatorStatus::SUCCESS) {
      CompleteMakeCredentialRequest(add_id_status);
      return;
    }
    // `ValidateAppidExtension` must have set a value to use. If not, it would
    // be a security bug, so crashing seems appropriate here.
    CHECK(!appid_exclude->empty());
  }

  // If there is an active webAuthenticationProxy extension, let it handle the
  // request.
  WebAuthenticationRequestProxy* proxy =
      GetWebAuthnRequestProxyIfActive(caller_origin);
  if (proxy) {
    if (options->remote_desktop_client_override) {
      // Don't allow proxying of an already proxied request.
      CompleteMakeCredentialRequest(
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    }
    options->remote_desktop_client_override =
        blink::mojom::RemoteDesktopClientOverride::New(
            /*origin=*/caller_origin_,
            /*same_origin_with_ancestors=*/!is_cross_origin_iframe);
    pending_proxied_request_id_ = proxy->SignalCreateRequest(
        options,
        base::BindOnce(&AuthenticatorCommonImpl::OnMakeCredentialProxyResponse,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  // Let the embedder override the RP ID to use for the request. In practice
  // this rewrites the RP ID that Chrome extensions use.
  absl::optional<std::string> rp_id_override =
      GetWebAuthenticationDelegate()->MaybeGetRelyingPartyIdOverride(
          options->relying_party.id, caller_origin);
  if (rp_id_override) {
    options->relying_party.id = *rp_id_override;
    relying_party_id_ = *rp_id_override;
  }
  request_delegate_->SetRelyingPartyId(relying_party_id_);
  request_delegate_->SetUserEntityForMakeCredentialRequest(options->user);

  device::fido_filter::MaybeInitialize();
  switch (device::fido_filter::Evaluate(
      device::fido_filter::Operation::MAKE_CREDENTIAL, relying_party_id_,
      /*device=*/absl::nullopt,
      /*id=*/absl::nullopt)) {
    case device::fido_filter::Action::ALLOW:
      break;
    case device::fido_filter::Action::NO_ATTESTATION:
      // This will be handled by the request handler.
      break;
    case device::fido_filter::Action::BLOCK:
      CompleteMakeCredentialRequest(
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
  }

  if (status != blink::mojom::AuthenticatorStatus::SUCCESS) {
    CompleteMakeCredentialRequest(status);
    return;
  }

  if (!IsFocused()) {
    CompleteMakeCredentialRequest(
        blink::mojom::AuthenticatorStatus::NOT_FOCUSED);
    return;
  }

  const device::AuthenticatorSelectionCriteria
      authenticator_selection_criteria =
          options->authenticator_selection
              ? *options->authenticator_selection
              : device::AuthenticatorSelectionCriteria();
  make_credential_options_ =
      device::MakeCredentialOptions(authenticator_selection_criteria);

  const bool might_create_resident_key =
      make_credential_options_->resident_key !=
      device::ResidentKeyRequirement::kDiscouraged;
  if (might_create_resident_key &&
      !GetWebAuthenticationDelegate()->SupportsResidentKeys(
          GetRenderFrameHost())) {
    if (make_credential_options_->resident_key ==
        device::ResidentKeyRequirement::kRequired) {
      CompleteMakeCredentialRequest(
          blink::mojom::AuthenticatorStatus::RESIDENT_CREDENTIALS_UNSUPPORTED);
      return;
    }
    // Downgrade 'preferred' to 'discouraged'.
    make_credential_options_->resident_key =
        device::ResidentKeyRequirement::kDiscouraged;
  }

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
      (!might_create_resident_key &&
       options->protection_policy == blink::mojom::ProtectionPolicy::NONE) ||
      // UV_REQUIRED only makes sense if UV is required overall.
      (options->protection_policy ==
           blink::mojom::ProtectionPolicy::UV_REQUIRED &&
       authenticator_selection_criteria.user_verification_requirement !=
           device::UserVerificationRequirement::kRequired)) {
    CompleteMakeCredentialRequest(
        blink::mojom::AuthenticatorStatus::PROTECTION_POLICY_INCONSISTENT);
    return;
  }

  absl::optional<device::CredProtectRequest> cred_protect_request =
      ProtectionPolicyToCredProtect(options->protection_policy,
                                    *make_credential_options_);
  if (cred_protect_request) {
    make_credential_options_->cred_protect_request = {
        {*cred_protect_request, options->enforce_protection_policy}};
  }

  // Touch-to-Autofill should be proxied without UI.
  if (disable_ui_) {
    request_delegate_->DisableUI();
  }

  // Assemble clientDataJSON.
  ClientDataJsonParams client_data_json_params(
      ClientDataRequestType::kWebAuthnCreate, caller_origin_,
      options->challenge, is_cross_origin_iframe);
  if (options->remote_desktop_client_override) {
    client_data_json_params.origin =
        options->remote_desktop_client_override->origin;
    client_data_json_params.is_cross_origin_iframe =
        !options->remote_desktop_client_override->same_origin_with_ancestors;
  }
  client_data_json_ = BuildClientDataJson(std::move(client_data_json_params));

  ctap_make_credential_request_ = device::CtapMakeCredentialRequest(
      client_data_json_, options->relying_party, options->user,
      device::PublicKeyCredentialParams(options->public_key_parameters));

  ctap_make_credential_request_->exclude_list = options->exclude_credentials;
  if (options->prf_enable) {
    requested_extensions_.insert(RequestExtension::kPRF);
    ctap_make_credential_request_->hmac_secret = true;
  }
  if (options->hmac_create_secret) {
    requested_extensions_.insert(RequestExtension::kHMACSecret);
    ctap_make_credential_request_->hmac_secret = true;
  }
  if (options->cred_props) {
    requested_extensions_.insert(RequestExtension::kCredProps);
  }
  if (options->large_blob_enable != device::LargeBlobSupport::kNotRequested) {
    requested_extensions_.insert(RequestExtension::kLargeBlobEnable);
  }
  if (options->cred_blob) {
    requested_extensions_.insert(RequestExtension::kCredBlob);
    ctap_make_credential_request_->cred_blob = *options->cred_blob;
  }
  if (options->min_pin_length_requested) {
    requested_extensions_.insert(RequestExtension::kMinPINLength);
    ctap_make_credential_request_->min_pin_length_requested = true;
  }
  make_credential_options_->large_blob_support = options->large_blob_enable;
  ctap_make_credential_request_->app_id_exclude = std::move(appid_exclude);
  make_credential_options_->is_off_the_record_context =
      GetBrowserContext()->IsOffTheRecord();
  if (options->device_public_key) {
    requested_extensions_.insert(RequestExtension::kDevicePublicKey);
    ctap_make_credential_request_->device_public_key.emplace();
    device::DevicePublicKeyRequest& device_public_key =
        ctap_make_credential_request_->device_public_key.value();
    device_public_key.attestation = options->device_public_key->attestation;
    device_public_key.attestation_formats =
        options->device_public_key->attestation_formats;

    device_public_key_attestation_requested_ =
        device_public_key.attestation !=
        device::AttestationConveyancePreference::kNone;

    switch (device_public_key.attestation) {
      // DPK attestation is currently an enterprise-only feature. Non-enterprise
      // values are mapped to "none".
      case device::AttestationConveyancePreference::kIndirect:
      case device::AttestationConveyancePreference::kDirect:
      case device::AttestationConveyancePreference::kNone:
        device_public_key.attestation =
            device::AttestationConveyancePreference::kNone;
        device_public_key.attestation_formats.clear();
        break;

      case device::AttestationConveyancePreference::
          kEnterpriseIfRPListedOnAuthenticator:
        if (GetWebAuthenticationDelegate()->ShouldPermitIndividualAttestation(
                GetBrowserContext(), caller_origin, relying_party_id_)) {
          device_public_key.attestation = device::
              AttestationConveyancePreference::kEnterpriseApprovedByBrowser;
        }
        break;

      case device::AttestationConveyancePreference::
          kEnterpriseApprovedByBrowser:
        // Enterprise attestation should not have been approved by this point.
        NOTREACHED();
        return;
    }
  }

  // Compute the effective attestation conveyance preference.
  device::AttestationConveyancePreference attestation = options->attestation;
  // Enterprise attestation should not have been approved by this point.
  DCHECK_NE(
      attestation,
      device::AttestationConveyancePreference::kEnterpriseApprovedByBrowser);
  if (attestation == device::AttestationConveyancePreference::
                         kEnterpriseIfRPListedOnAuthenticator &&
      GetWebAuthenticationDelegate()->ShouldPermitIndividualAttestation(
          GetBrowserContext(), caller_origin, relying_party_id_)) {
    attestation =
        device::AttestationConveyancePreference::kEnterpriseApprovedByBrowser;
  }
  ctap_make_credential_request_->attestation_preference = attestation;

  StartMakeCredentialRequest(/*allow_skipping_pin_touch=*/true);
}

void AuthenticatorCommonImpl::GetAssertion(
    url::Origin caller_origin,
    blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
    blink::mojom::PaymentOptionsPtr payment_options,
    blink::mojom::Authenticator::GetAssertionCallback callback) {
  if (has_pending_request_) {
    std::move(callback).Run(blink::mojom::AuthenticatorStatus::PENDING_REQUEST,
                            nullptr, nullptr);
    return;
  }
  has_pending_request_ = true;

  DCHECK(get_assertion_response_callback_.is_null());
  get_assertion_response_callback_ = std::move(callback);

  if (!options->is_conditional) {
    BeginRequestTimeout(options->timeout);
  }

  WebAuthRequestSecurityChecker::RequestType request_type =
      payment_options.is_null()
          ? WebAuthRequestSecurityChecker::RequestType::kGetAssertion
          : WebAuthRequestSecurityChecker::RequestType::
                kGetPaymentCredentialAssertion;
  if (!payment_options.is_null() && options->allow_credentials.empty()) {
    CompleteGetAssertionRequest(
        blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
    NOTREACHED();
    return;
  }
  bool is_cross_origin_iframe = false;
  blink::mojom::AuthenticatorStatus status =
      security_checker_->ValidateAncestorOrigins(caller_origin, request_type,
                                                 &is_cross_origin_iframe);
  if (status != blink::mojom::AuthenticatorStatus::SUCCESS) {
    CompleteGetAssertionRequest(status);
    return;
  }

  if (!security_checker_->DeduplicateCredentialDescriptorListAndValidateLength(
          &options->allow_credentials)) {
    mojo::ReportBadMessage("invalid allow_credentials length");
    CompleteGetAssertionRequest(
        blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
    return;
  }

  status = security_checker_->ValidateDomainAndRelyingPartyID(
      caller_origin, options->relying_party_id, request_type,
      options->extensions->remote_desktop_client_override);
  if (status != blink::mojom::AuthenticatorStatus::SUCCESS) {
    CompleteGetAssertionRequest(status);
    return;
  }

  request_delegate_ = MaybeCreateRequestDelegate();
  if (!request_delegate_) {
    CompleteGetAssertionRequest(
        blink::mojom::AuthenticatorStatus::PENDING_REQUEST);
    return;
  }
  if (!request_delegate_->IsVirtualEnvironmentEnabled() &&
      !disable_tls_check_ &&
      !GetWebAuthenticationDelegate()->IsSecurityLevelAcceptableForWebAuthn(
          GetRenderFrameHost(), caller_origin)) {
    CompleteGetAssertionRequest(
        blink::mojom::AuthenticatorStatus::CERTIFICATE_ERROR);
    return;
  }

  caller_origin_ = caller_origin;
  relying_party_id_ = options->relying_party_id;

  if (options->extensions->appid) {
    requested_extensions_.insert(RequestExtension::kAppID);
    std::string app_id;
    auto add_id_status = security_checker_->ValidateAppIdExtension(
        *options->extensions->appid, caller_origin,
        options->extensions->remote_desktop_client_override, &app_id);
    if (add_id_status != blink::mojom::AuthenticatorStatus::SUCCESS) {
      CompleteGetAssertionRequest(add_id_status);
      return;
    }
    // `ValidateAppidExtension` must have set a value to use. If not, it would
    // be a security bug, so crashing seems appropriate here.
    CHECK(!app_id.empty());
    app_id_ = app_id;
  }

  WebAuthenticationRequestProxy* proxy =
      GetWebAuthnRequestProxyIfActive(caller_origin);
  if (proxy) {
    if (options->is_conditional ||
        (options->extensions->remote_desktop_client_override)) {
      // Don't allow proxying of an already proxied or conditional request.
      CompleteGetAssertionRequest(
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    }
    options->extensions->remote_desktop_client_override =
        blink::mojom::RemoteDesktopClientOverride::New(
            /*origin=*/caller_origin_,
            /*same_origin_with_ancestors=*/!is_cross_origin_iframe);
    pending_proxied_request_id_ = proxy->SignalGetRequest(
        options,
        base::BindOnce(&AuthenticatorCommonImpl::OnGetAssertionProxyResponse,
                       weak_factory_.GetWeakPtr()));
    return;
  }

  // Let the embedder override the RP ID to use for the request. In practice
  // this rewrites the RP ID that Chrome extension use.
  absl::optional<std::string> rp_id_override =
      GetWebAuthenticationDelegate()->MaybeGetRelyingPartyIdOverride(
          options->relying_party_id, caller_origin);
  if (rp_id_override) {
    options->relying_party_id = *rp_id_override;
    relying_party_id_ = *rp_id_override;
  }
  request_delegate_->SetRelyingPartyId(relying_party_id_);

  // Assemble clientDataJSON.
  ClientDataJsonParams client_data_json_params(
      ClientDataRequestType::kWebAuthnGet, caller_origin, options->challenge,
      is_cross_origin_iframe);
  if (payment_options) {
    client_data_json_params.type = ClientDataRequestType::kPaymentGet;
    client_data_json_params.payment_options = std::move(payment_options);
    client_data_json_params.payment_rp = relying_party_id_;
    client_data_json_params.payment_top_origin = GetRenderFrameHost()
                                                     ->GetOutermostMainFrame()
                                                     ->GetLastCommittedOrigin()
                                                     .Serialize();
  } else if (options->extensions->remote_desktop_client_override) {
    client_data_json_params.origin =
        options->extensions->remote_desktop_client_override->origin;
    client_data_json_params.is_cross_origin_iframe =
        !options->extensions->remote_desktop_client_override
             ->same_origin_with_ancestors;
  }
  client_data_json_ = BuildClientDataJson(std::move(client_data_json_params));

  device::fido_filter::MaybeInitialize();
  if (device::fido_filter::Evaluate(
          device::fido_filter::Operation::GET_ASSERTION, relying_party_id_,
          /*device=*/absl::nullopt,
          /*id=*/absl::nullopt) == device::fido_filter::Action::BLOCK) {
    CompleteGetAssertionRequest(
        blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
    return;
  }

  if (disable_ui_) {
    DCHECK(!options->is_conditional);
    request_delegate_->DisableUI();
  }

  request_delegate_->SetConditionalRequest(options->is_conditional);

  if (options->is_conditional && !options->allow_credentials.empty()) {
    // Conditional mediation requests can only be fulfilled by discoverable
    // credentials. The provided allowCredentials list is stripped and will be
    // used to filter returned passkeys
    request_delegate_->SetCredentialIdFilter(
        std::move(options->allow_credentials));
    options->allow_credentials =
        std::vector<device::PublicKeyCredentialDescriptor>();
  }

  if (options->allow_credentials.empty()) {
    if (!GetWebAuthenticationDelegate()->SupportsResidentKeys(
            GetRenderFrameHost())) {
      CompleteGetAssertionRequest(
          blink::mojom::AuthenticatorStatus::RESIDENT_CREDENTIALS_UNSUPPORTED);
      return;
    }
    discoverable_credential_request_ = true;
  }

  if (options->extensions->large_blob_read &&
      options->extensions->large_blob_write) {
    CompleteGetAssertionRequest(
        blink::mojom::AuthenticatorStatus::CANNOT_READ_AND_WRITE_LARGE_BLOB);
    return;
  }

  if (options->extensions->large_blob_read) {
    requested_extensions_.insert(RequestExtension::kLargeBlobRead);
  } else if (options->extensions->large_blob_write) {
    if (options->allow_credentials.size() != 1) {
      CompleteGetAssertionRequest(blink::mojom::AuthenticatorStatus::
                                      INVALID_ALLOW_CREDENTIALS_FOR_LARGE_BLOB);
      return;
    }
    requested_extensions_.insert(RequestExtension::kLargeBlobWrite);
  }

  ctap_get_assertion_request_ =
      CreateCtapGetAssertionRequest(client_data_json_, options, app_id_);
  ctap_get_assertion_options_.emplace();
  ctap_get_assertion_options_->is_off_the_record_context =
      GetBrowserContext()->IsOffTheRecord();

  if (options->extensions->prf) {
    requested_extensions_.insert(RequestExtension::kPRF);

    bool is_first = true;
    absl::optional<std::vector<uint8_t>> last_id;
    // TODO(agl): should match the credential IDs from the allow list, which
    // will also limit the size to the size of the allow list.
    for (const auto& prf_input_from_renderer :
         options->extensions->prf_inputs) {
      device::PRFInput prf_input;

      // This statement enforces invariants that should be established by the
      // renderer.
      if (
          // Only the first element in the vector may be the default.
          (!is_first && !prf_input_from_renderer->id) ||
          // The PRF inputs must be sorted by credential ID to show that there
          // are no duplicates.
          (last_id.has_value() && prf_input_from_renderer->id.has_value() &&
           *last_id >= *prf_input_from_renderer->id) ||
          // The lengths are specified in authenticator.mojom, so hopefully Mojo
          // enforces them too.
          prf_input_from_renderer->first.size() != prf_input.salt1.size() ||
          (prf_input_from_renderer->second &&
           prf_input_from_renderer->second->size() != prf_input.salt1.size())) {
        NOTREACHED();

        CompleteGetAssertionRequest(
            blink::mojom::AuthenticatorStatus::UNKNOWN_ERROR);
        return;
      }
      is_first = false;
      last_id = prf_input_from_renderer->id;

      if (prf_input_from_renderer->id) {
        prf_input.credential_id = std::move(*prf_input_from_renderer->id);
      }

      memcpy(prf_input.salt1.data(), prf_input_from_renderer->first.data(),
             prf_input.salt1.size());
      if (prf_input_from_renderer->second) {
        prf_input.salt2.emplace();
        memcpy(prf_input.salt2->data(), prf_input_from_renderer->second->data(),
               prf_input.salt2->size());
      }

      ctap_get_assertion_options_->prf_inputs.emplace_back(
          std::move(prf_input));
    }
  }

  if (options->extensions->device_public_key) {
    requested_extensions_.insert(RequestExtension::kDevicePublicKey);
    ctap_get_assertion_request_->device_public_key.emplace();
    device::DevicePublicKeyRequest& device_public_key =
        ctap_get_assertion_request_->device_public_key.value();
    device_public_key.attestation =
        options->extensions->device_public_key->attestation;
    device_public_key.attestation_formats =
        options->extensions->device_public_key->attestation_formats;

    switch (device_public_key.attestation) {
      // DPK attestation is currently an enterprise-only feature. Non-enterprise
      // values are mapped to "none". There's no prompting for getAssertion
      // either so only policy-configured enterprise attestation works for this
      // call.
      case device::AttestationConveyancePreference::
          kEnterpriseIfRPListedOnAuthenticator:
        device_public_key.attestation = device::
            AttestationConveyancePreference::kEnterpriseApprovedByBrowser;
        [[fallthrough]];

      case device::AttestationConveyancePreference::kIndirect:
      case device::AttestationConveyancePreference::kDirect:
        if (GetWebAuthenticationDelegate()->ShouldPermitIndividualAttestation(
                GetBrowserContext(), caller_origin, relying_party_id_)) {
          break;
        }
        [[fallthrough]];

      case device::AttestationConveyancePreference::kNone:
        device_public_key.attestation =
            device::AttestationConveyancePreference::kNone;
        device_public_key.attestation_formats.clear();
        break;

      case device::AttestationConveyancePreference::
          kEnterpriseApprovedByBrowser:
        // This should never come from the renderer.
        mojo::ReportBadMessage("invalid devicePubKey attestation value");
        CompleteGetAssertionRequest(
            blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
        break;
    }
  }

  if (options->extensions->get_cred_blob) {
    requested_extensions_.insert(RequestExtension::kGetCredBlob);
    ctap_get_assertion_request_->get_cred_blob = true;
  }

  ctap_get_assertion_options_->large_blob_read =
      options->extensions->large_blob_read;
  ctap_get_assertion_options_->large_blob_write =
      options->extensions->large_blob_write;

  StartGetAssertionRequest(/*allow_skipping_pin_touch=*/true);
}

void AuthenticatorCommonImpl::IsUserVerifyingPlatformAuthenticatorAvailable(
    url::Origin caller_origin,
    blink::mojom::Authenticator::
        IsUserVerifyingPlatformAuthenticatorAvailableCallback callback) {
  WebAuthenticationRequestProxy* proxy =
      GetWebAuthnRequestProxyIfActive(caller_origin);
  if (proxy) {
    // Note that IsUvpaa requests can interleave with MakeCredential or
    // GetAssertion, and cannot be cancelled. Thus, we do not set
    // `pending_proxied_request_id_` here.
    proxy->SignalIsUvpaaRequest(std::move(callback));
    return;
  }

  // Check for a delegate override. Chrome overrides IsUVPAA() in Guest mode.
  absl::optional<bool> is_uvpaa_override =
      GetWebAuthenticationDelegate()
          ->IsUserVerifyingPlatformAuthenticatorAvailableOverride(
              GetRenderFrameHost());
  if (is_uvpaa_override) {
    std::move(callback).Run(*is_uvpaa_override);
    return;
  }

  // Record IsUVPAA result in a UMA metric, but only if they're not the
  // WebAuthenticationDelegate override value, so that results from the testing
  // API and disabling in Guest/Off-The-Record profiles aren't counted.
  auto uma_decorated_callback =
      base::BindOnce([](bool available) {
        base::UmaHistogramBoolean(
            "WebAuthentication.IsUVPlatformAuthenticatorAvailable2", available);
        return available;
      }).Then(std::move(callback));

#if BUILDFLAG(IS_MAC)
  IsUVPlatformAuthenticatorAvailable(GetBrowserContext(),
                                     std::move(uma_decorated_callback));
#elif BUILDFLAG(IS_WIN)
  IsUVPlatformAuthenticatorAvailable(GetBrowserContext()->IsOffTheRecord(),
                                     std::move(uma_decorated_callback));
#elif BUILDFLAG(IS_CHROMEOS)
  IsUVPlatformAuthenticatorAvailable(std::move(uma_decorated_callback));
#else
  std::move(uma_decorated_callback).Run(false);
#endif
}

void AuthenticatorCommonImpl::IsConditionalMediationAvailable(
    url::Origin caller_origin,
    blink::mojom::Authenticator::IsConditionalMediationAvailableCallback
        callback) {
  // Conditional mediation is always supported if the virtual environment is
  // providing a platform authenticator.
  absl::optional<bool> embedder_isuvpaa_override =
      GetWebAuthenticationDelegate()
          ->IsUserVerifyingPlatformAuthenticatorAvailableOverride(
              GetRenderFrameHost());
  if (embedder_isuvpaa_override.has_value()) {
    std::move(callback).Run(*embedder_isuvpaa_override);
    return;
  }

  if (GetWebAuthnRequestProxyIfActive(caller_origin)) {
    // Conditional requests cannot be proxied, signal the feature as
    // unavailable.
    std::move(callback).Run(false);
    return;
  }
#if BUILDFLAG(IS_MAC)
  std::move(callback).Run(true);
#elif BUILDFLAG(IS_WIN)
  device::WinWebAuthnApiAuthenticator::IsConditionalMediationAvailable(
      device::WinWebAuthnApi::GetDefault(), std::move(callback));
#else
  std::move(callback).Run(false);
#endif
}

void AuthenticatorCommonImpl::Cancel() {
  CancelWithStatus(blink::mojom::AuthenticatorStatus::ABORT_ERROR);
}

void AuthenticatorCommonImpl::OnRegisterResponse(
    device::MakeCredentialStatus status_code,
    absl::optional<device::AuthenticatorMakeCredentialResponse> response_data,
    const device::FidoAuthenticator* authenticator) {
  if (!request_handler_) {
    // Either the callback was called immediately and |request_handler_| has not
    // yet been assigned (this is a bug), or a navigation caused the request to
    // be canceled while a callback was enqueued.
    return;
  }

  switch (status_code) {
    case device::MakeCredentialStatus::kUserConsentButCredentialExcluded:
    case device::MakeCredentialStatus::kWinInvalidStateError:
      // Duplicate registration: the new credential would be created on an
      // authenticator that already contains one of the credentials in
      // |exclude_credentials|. If the request specified that only a platform
      // authenticator was acceptable then we don't show an error message
      // because there's no other authenticator that could be used for this
      // request. Instead the RP learns of the result via the distinctive
      // InvalidStateError result. This tells them that the platform
      // authenticator is already registered with one of the credential IDs that
      // they already know about.
      //
      // Windows already behaves like this and so its representation of
      // InvalidStateError is handled this way too.
      if (make_credential_options_->authenticator_attachment ==
              device::AuthenticatorAttachment::kPlatform ||
          status_code == device::MakeCredentialStatus::kWinInvalidStateError) {
        CompleteMakeCredentialRequest(
            blink::mojom::AuthenticatorStatus::CREDENTIAL_EXCLUDED, nullptr,
            nullptr, Focus::kDoCheck);
      } else {
        SignalFailureToRequestDelegate(
            AuthenticatorRequestClientDelegate::InterestingFailureReason::
                kKeyAlreadyRegistered,
            blink::mojom::AuthenticatorStatus::CREDENTIAL_EXCLUDED);
      }
      return;
    case device::MakeCredentialStatus::kAuthenticatorResponseInvalid:
      // The response from the authenticator was corrupted.
      CompleteMakeCredentialRequest(
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR, nullptr,
          nullptr, Focus::kDoCheck);
      return;
    case device::MakeCredentialStatus::kHybridTransportError:
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kHybridTransportError,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::MakeCredentialStatus::kUserConsentDenied:
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kUserConsentDenied,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::MakeCredentialStatus::kSoftPINBlock:
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kSoftPINBlock,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::MakeCredentialStatus::kHardPINBlock:
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kHardPINBlock,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::MakeCredentialStatus::kAuthenticatorRemovedDuringPINEntry:
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kAuthenticatorRemovedDuringPINEntry,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::MakeCredentialStatus::kAuthenticatorMissingResidentKeys:
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kAuthenticatorMissingResidentKeys,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::MakeCredentialStatus::kAuthenticatorMissingUserVerification:
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kAuthenticatorMissingUserVerification,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::MakeCredentialStatus::kAuthenticatorMissingLargeBlob:
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kAuthenticatorMissingLargeBlob,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::MakeCredentialStatus::kNoCommonAlgorithms:
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kNoCommonAlgorithms,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::MakeCredentialStatus::kStorageFull:
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kStorageFull,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::MakeCredentialStatus::kWinNotAllowedError:
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kWinUserCancelled,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::MakeCredentialStatus::kSuccess:
      DCHECK(response_data.has_value());
      DCHECK(authenticator);

      absl::optional<device::FidoTransportProtocol> transport =
          authenticator->AuthenticatorTransport();
      bool is_transport_used_internal = false;
      bool is_transport_used_cable = false;
      if (transport) {
        is_transport_used_internal =
            *transport == device::FidoTransportProtocol::kInternal;
        is_transport_used_cable =
            *transport == device::FidoTransportProtocol::kHybrid;
      }

      absl::optional<device::DevicePublicKeyOutput> device_public_key_output =
          response_data->GetDevicePublicKeyResponse();
      const bool have_enterprise_attestation =
          response_data->enterprise_attestation_returned ||
          (device_public_key_output &&
           device_public_key_output->enterprise_attestation_returned);
      const bool device_public_key_included_attestation =
          device_public_key_output &&
          device_public_key_output->attestation_format !=
              device::kNoneAttestationValue;
      const auto attestation =
          ctap_make_credential_request_->attestation_preference;
      absl::optional<AttestationErasureOption> attestation_erasure;

      if (response_data->attestation_should_be_filtered &&
          !GetWebAuthenticationDelegate()->ShouldPermitIndividualAttestation(
              GetBrowserContext(), caller_origin_, relying_party_id_)) {
        attestation_erasure =
            AttestationErasureOption::kEraseAttestationAndAaguid;
      } else if (attestation == device::AttestationConveyancePreference::
                                    kEnterpriseApprovedByBrowser) {
        // If enterprise attestation was approved by policy then it can be
        // returned immediately.
        attestation_erasure = AttestationErasureOption::kIncludeAttestation;
      } else if (attestation == device::AttestationConveyancePreference::
                                    kEnterpriseIfRPListedOnAuthenticator &&
                 !response_data->enterprise_attestation_returned) {
        // If enterprise attestation was requested, not approved by policy, and
        // not approved by the authenticator, then any attestation is stripped.
        attestation_erasure =
            AttestationErasureOption::kEraseAttestationAndAaguid;
      } else if (is_transport_used_cable) {
        // Attestation is not returned when caBLEv2 is used, but the AAGUID is
        // maintained.
        attestation_erasure =
            AttestationErasureOption::kEraseAttestationButIncludeAaguid;
      } else if (is_transport_used_internal) {
        // Direct attestation from platform authenticators is known to be
        // privacy preserving, so we always return it when requested. Also,
        // counter to what the WebAuthn spec says, we do not erase the AAGUID
        // even when attestation wasn't requested.
        attestation_erasure =
            attestation != device::AttestationConveyancePreference::kNone
                ? AttestationErasureOption::kIncludeAttestation
                : AttestationErasureOption::kEraseAttestationButIncludeAaguid;
      } else if (attestation ==
                     device::AttestationConveyancePreference::kNone &&
                 response_data->attestation_object.IsSelfAttestation()) {
        attestation_erasure = AttestationErasureOption::kIncludeAttestation;
      } else if (attestation ==
                 device::AttestationConveyancePreference::kNone) {
        attestation_erasure =
            AttestationErasureOption::kEraseAttestationAndAaguid;
      }

      if (attestation_erasure.has_value() &&
          // If a DPK attestation was requested then we show a prompt. (If
          // the RP ID is allowlisted by policy then the prompt will be
          // resolved immediately and never actually shown.)
          !device_public_key_attestation_requested_) {
        CompleteMakeCredentialRequest(
            blink::mojom::AuthenticatorStatus::SUCCESS,
            CreateMakeCredentialResponse(std::move(*response_data),
                                         *attestation_erasure),
            nullptr, Focus::kDoCheck);
      } else {
        awaiting_attestation_response_ = true;
        request_delegate_->ShouldReturnAttestation(
            relying_party_id_, authenticator, have_enterprise_attestation,
            base::BindOnce(
                &AuthenticatorCommonImpl::OnRegisterResponseAttestationDecided,
                weak_factory_.GetWeakPtr(),
                attestation_erasure.value_or(
                    AttestationErasureOption::kIncludeAttestation),
                device_public_key_output.has_value(),
                device_public_key_included_attestation,
                std::move(*response_data)));
      }

      return;
  }
  NOTREACHED();
}

void AuthenticatorCommonImpl::OnRegisterResponseAttestationDecided(
    AttestationErasureOption attestation_erasure,
    const bool has_device_public_key_output,
    const bool device_public_key_included_attestation,
    device::AuthenticatorMakeCredentialResponse response_data,
    bool attestation_permitted) {
  awaiting_attestation_response_ = false;
  if (!request_handler_) {
    // The request has already been cleaned up, probably because a navigation
    // occurred while the permissions prompt was pending.
    return;
  }

  if (!attestation_permitted) {
    attestation_erasure = AttestationErasureOption::kEraseAttestationAndAaguid;
  }

  // The check for IsAttestationCertificateInappropriatelyIdentifying is
  // performed after the permissions prompt, even though we know the answer
  // before, because this still effectively discloses the make & model of
  // the authenticator: If an RP sees a "none" attestation from Chrome after
  // requesting direct attestation then it knows that it was one of the
  // tokens with inappropriate certs.
  if (response_data.attestation_object
          .IsAttestationCertificateInappropriatelyIdentifying() &&
      !GetWebAuthenticationDelegate()->ShouldPermitIndividualAttestation(
          GetBrowserContext(), caller_origin_, relying_party_id_)) {
    // The attestation response is incorrectly individually identifiable, but
    // the consent is for make & model information about a token, not for
    // individually-identifiable information. Erase the attestation to stop it
    // begin a tracking signal.

    // The only way to get the underlying attestation will be to list the RP ID
    // in the enterprise policy, because that enables the individual attestation
    // bit in the register request and permits individual attestation generally.
    attestation_erasure = AttestationErasureOption::kEraseAttestationAndAaguid;
  }

  CompleteMakeCredentialRequest(
      blink::mojom::AuthenticatorStatus::SUCCESS,
      CreateMakeCredentialResponse(std::move(response_data),
                                   attestation_erasure),
      nullptr, Focus::kDoCheck);
}

void AuthenticatorCommonImpl::OnSignResponse(
    device::GetAssertionStatus status_code,
    absl::optional<std::vector<device::AuthenticatorGetAssertionResponse>>
        response_data) {
  DCHECK(!response_data || !response_data->empty());  // empty vector is invalid
  if (!request_handler_) {
    // Either the callback was called immediately and |request_handler_| has not
    // yet been assigned (this is a bug), or a navigation caused the request to
    // be canceled while a callback was enqueued.
    return;
  }

  switch (status_code) {
    case device::GetAssertionStatus::kUserConsentButCredentialNotRecognized:
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kKeyNotRegistered,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::GetAssertionStatus::kAuthenticatorResponseInvalid:
      // The response from the authenticator was corrupted.
      CompleteGetAssertionRequest(
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::GetAssertionStatus::kUserConsentDenied:
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kUserConsentDenied,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::GetAssertionStatus::kSoftPINBlock:
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kSoftPINBlock,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::GetAssertionStatus::kHardPINBlock:
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kHardPINBlock,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::GetAssertionStatus::kAuthenticatorRemovedDuringPINEntry:
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kAuthenticatorRemovedDuringPINEntry,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::GetAssertionStatus::kAuthenticatorMissingResidentKeys:
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kAuthenticatorMissingResidentKeys,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::GetAssertionStatus::kAuthenticatorMissingUserVerification:
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kAuthenticatorMissingUserVerification,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::GetAssertionStatus::kWinNotAllowedError:
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kWinUserCancelled,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::GetAssertionStatus::kHybridTransportError:
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kHybridTransportError,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::GetAssertionStatus::kSuccess:
      break;
  }

  DCHECK_EQ(status_code, device::GetAssertionStatus::kSuccess);
  DCHECK(response_data.has_value());

  // Show an account picker for discoverable credential requests (empty allow
  // lists). Responses with a single credential are considered pre-selected if
  // one of the following is true:
  // - The authenticator omitted user entity information because only one
  // credential matched (only valid in CTAP 2.0).
  // - The `userSelected` flag is set, because the user chose an account on an
  // integrated authenticator UI (CTAP 2.1).
  // - The user already pre-selected a platform authenticator credential from
  // browser UI prior to the actual GetAssertion request. (The request handler
  // set the `userSelected` flag in this case.)
  if (response_data->size() == 1) {
    const device::AuthenticatorGetAssertionResponse& response =
        response_data->at(0);
    if (!discoverable_credential_request_ || response.user_selected ||
        !response.user_entity || !response.user_entity->name ||
        !response.user_entity->display_name) {
      OnAccountSelected(std::move(response_data->at(0)));
      return;
    }
  }

  // Discoverable credential request without preselection UI. Show an account
  // picker.
  std::vector<device::PublicKeyCredentialUserEntity> users_list;
  users_list.reserve(response_data->size());
  for (const auto& response : *response_data) {
    if (response.user_entity) {
      users_list.push_back(*response.user_entity);
    }
  }
  request_delegate_->SelectAccount(
      std::move(*response_data),
      base::BindOnce(&AuthenticatorCommonImpl::OnAccountSelected,
                     weak_factory_.GetWeakPtr()));
}

void AuthenticatorCommonImpl::OnAccountSelected(
    device::AuthenticatorGetAssertionResponse response) {
  CompleteGetAssertionRequest(blink::mojom::AuthenticatorStatus::SUCCESS,
                              CreateGetAssertionResponse(std::move(response)));
}

void AuthenticatorCommonImpl::SignalFailureToRequestDelegate(
    AuthenticatorRequestClientDelegate::InterestingFailureReason reason,
    blink::mojom::AuthenticatorStatus status) {
  error_awaiting_user_acknowledgement_ = status;

  // The UI may decide to end the request immediately, or after user
  // confirmation. Either way stop discoveries and authenticators now.
  if (request_handler_) {
    request_handler_->StopDiscoveries();
    request_handler_->CancelActiveAuthenticators();
  }

  if (request_delegate_->DoesBlockRequestOnFailure(reason)) {
    // The UI may have decided to start the request over. Thus do not assume
    // anything about the state here.
    return;
  }

  // The UI wishes the end the request immediately.
  CancelWithStatus(error_awaiting_user_acknowledgement_);
}

void AuthenticatorCommonImpl::BeginRequestTimeout(
    absl::optional<base::TimeDelta> timeout) {
  timer_->Start(FROM_HERE, AdjustTimeout(timeout, GetRenderFrameHost()),
                base::BindOnce(&AuthenticatorCommonImpl::OnTimeout,
                               weak_factory_.GetWeakPtr()));
}

// TODO(crbug.com/814418): Add web tests to verify timeouts are
// indistinguishable from NOT_ALLOWED_ERROR cases.
void AuthenticatorCommonImpl::OnTimeout() {
  if (awaiting_attestation_response_) {
    awaiting_attestation_response_ = false;
  }

  DCHECK(request_delegate_);
  SignalFailureToRequestDelegate(
      AuthenticatorRequestClientDelegate::InterestingFailureReason::kTimeout,
      blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

void AuthenticatorCommonImpl::CancelWithStatus(
    blink::mojom::AuthenticatorStatus status) {
  DCHECK(!make_credential_response_callback_ ||
         !get_assertion_response_callback_);
  if (pending_proxied_request_id_) {
    WebAuthenticationRequestProxy* proxy =
        GetWebAuthenticationDelegate()->MaybeGetRequestProxy(
            GetBrowserContext(), caller_origin_);
    // As long as `pending_proxied_request_id_` is set, there should be an
    // active request proxy. Deactivation of the proxy would have invoked
    // `OnMakeCredentialProxyResponse()` or `OnGetAssertionProxyResponse()`, and
    // cleared `pending_proxied_request_id_`
    DCHECK(proxy);
    proxy->CancelRequest(*pending_proxied_request_id_);
  }
  if (make_credential_response_callback_) {
    CompleteMakeCredentialRequest(status);
  } else if (get_assertion_response_callback_) {
    CompleteGetAssertionRequest(status);
  }
}

void AuthenticatorCommonImpl::OnCancelFromUI() {
  CancelWithStatus(error_awaiting_user_acknowledgement_);
}

blink::mojom::MakeCredentialAuthenticatorResponsePtr
AuthenticatorCommonImpl::CreateMakeCredentialResponse(
    device::AuthenticatorMakeCredentialResponse response_data,
    AttestationErasureOption attestation_erasure) {
  auto response = blink::mojom::MakeCredentialAuthenticatorResponse::New();
  auto common_info = blink::mojom::CommonCredentialInfo::New();
  common_info->client_data_json.assign(client_data_json_.begin(),
                                       client_data_json_.end());
  common_info->raw_id = response_data.attestation_object.GetCredentialId();
  common_info->id = Base64UrlEncode(common_info->raw_id);

  response->authenticator_attachment =
      response_data.transport_used
          ? device::AuthenticatorAttachmentFromTransport(
                *response_data.transport_used)
          : device::AuthenticatorAttachment::kAny;

  base::flat_set<device::FidoTransportProtocol> transports;
  // transports_authoritative tracks whether the contents of `transports` are
  // considered to be sufficient complete to report back to the website.
  bool transports_authoritative = false;

  if (response_data.transport_used) {
    transports.insert(*response_data.transport_used);
  }
  if (response_data.transports) {
    transports.insert(response_data.transports->begin(),
                      response_data.transports->end());
    transports_authoritative = true;
  }
  // Also include any transports from the attestation certificate.
  absl::optional<base::span<const uint8_t>> leaf_cert =
      response_data.attestation_object.attestation_statement()
          .GetLeafCertificate();
  if (leaf_cert) {
    transports_authoritative |=
        AddTransportsFromCertificate(*leaf_cert, &transports);
  }

  if (!transports_authoritative &&
      response_data.transport_used == device::FidoTransportProtocol::kHybrid) {
    // Windows doesn't provide transport data, but can provide the transport
    // used. If the transport was hybrid then we assume that ['hybrid',
    // 'internal'] is a reasonable set of transports.
    transports.insert(device::FidoTransportProtocol::kHybrid);
    transports.insert(device::FidoTransportProtocol::kInternal);
    transports_authoritative = true;
  }

  // The order of transports doesn't matter because Blink will sort the
  // resulting strings before returning them.
  if (transports_authoritative) {
    response->transports.assign(transports.begin(), transports.end());
  }

  bool did_modify_authenticator_data = false;
  switch (attestation_erasure) {
    case AttestationErasureOption::kIncludeAttestation:
      break;
    case AttestationErasureOption::kEraseAttestationButIncludeAaguid:
      did_modify_authenticator_data =
          response_data.attestation_object.EraseAttestationStatement(
              device::AttestationObject::AAGUID::kInclude);
      break;
    case AttestationErasureOption::kEraseAttestationAndAaguid:
      did_modify_authenticator_data =
          response_data.attestation_object.EraseAttestationStatement(
              device::AttestationObject::AAGUID::kErase);
      break;
  }

  if (did_modify_authenticator_data) {
    // The devicePubKey extension signs over the authenticator data so its
    // signature is now invalid and we have to remove the extension.
    response_data.attestation_object.EraseExtension(
        device::kExtensionDevicePublicKey);
  }

  bool did_create_hmac_secret = response_data.prf_enabled;
  bool did_store_cred_blob = false;
  absl::optional<std::vector<uint8_t>> device_public_key_authenticator_output;
  const absl::optional<cbor::Value>& maybe_extensions =
      response_data.attestation_object.authenticator_data().extensions();
  if (maybe_extensions) {
    DCHECK(maybe_extensions->is_map());
    const cbor::Value::MapValue& extensions = maybe_extensions->GetMap();

    if (!did_create_hmac_secret) {
      const auto hmac_secret_it =
          extensions.find(cbor::Value(device::kExtensionHmacSecret));
      if (hmac_secret_it != extensions.end() &&
          hmac_secret_it->second.is_bool() &&
          hmac_secret_it->second.GetBool()) {
        did_create_hmac_secret = true;
      }
    }

    const auto cred_blob_it =
        extensions.find(cbor::Value(device::kExtensionCredBlob));
    if (cred_blob_it != extensions.end() && cred_blob_it->second.is_bool() &&
        cred_blob_it->second.GetBool()) {
      did_store_cred_blob = true;
    }

    const auto device_public_key_it =
        extensions.find(cbor::Value(device::kExtensionDevicePublicKey));
    if (device_public_key_it != extensions.end() &&
        device_public_key_it->second.is_bytestring()) {
      device_public_key_authenticator_output =
          device_public_key_it->second.GetBytestring();
    }
  }

  for (const RequestExtension ext : requested_extensions_) {
    switch (ext) {
      case RequestExtension::kPRF:
        response->echo_prf = true;
        response->prf = did_create_hmac_secret;
        break;
      case RequestExtension::kHMACSecret:
        response->echo_hmac_create_secret = true;
        response->hmac_create_secret = did_create_hmac_secret;
        break;
      case RequestExtension::kCredProps:
        response->echo_cred_props = true;
        if (response_data.is_resident_key) {
          response->has_cred_props_rk = true;
          response->cred_props_rk = *response_data.is_resident_key;
        }
        break;
      case RequestExtension::kLargeBlobEnable:
        response->echo_large_blob = true;
        response->supports_large_blob =
            response_data.large_blob_type.has_value();
        break;
      case RequestExtension::kCredBlob:
        response->echo_cred_blob = true;
        response->cred_blob = did_store_cred_blob;
        break;
      case RequestExtension::kMinPINLength:
        // Ignore. The spec says[1] that there's no client (i.e. browser)
        // extension output (as opposed to the output in the returned
        // authenticator data). This may have been a mistake but it can always
        // be added later.
        // [1]
        // https://fidoalliance.org/specs/fido-v2.1-ps-20210615/fido-client-to-authenticator-protocol-v2.1-ps-20210615.html#sctn-minpinlength-extension
        break;
      case RequestExtension::kDevicePublicKey:
        if (device_public_key_authenticator_output &&
            response_data.device_public_key_signature) {
          DCHECK(!did_modify_authenticator_data);

          response->device_public_key =
              blink::mojom::DevicePublicKeyResponse::New();
          response->device_public_key->authenticator_output =
              std::move(*device_public_key_authenticator_output);
          response->device_public_key->signature =
              *response_data.device_public_key_signature;
        }
        break;
      case RequestExtension::kAppID:
      case RequestExtension::kLargeBlobRead:
      case RequestExtension::kLargeBlobWrite:
      case RequestExtension::kGetCredBlob:
        NOTREACHED();
        break;
    }
  }

  response->attestation_object =
      response_data.GetCBOREncodedAttestationObject();
  common_info->authenticator_data =
      response_data.attestation_object.authenticator_data()
          .SerializeToByteArray();
  response->info = std::move(common_info);

  const device::PublicKey* public_key =
      response_data.attestation_object.authenticator_data()
          .attested_data()
          ->public_key();
  response->public_key_algo = public_key->algorithm;
  const absl::optional<std::vector<uint8_t>>& public_key_der =
      public_key->der_bytes;
  if (public_key_der) {
    response->public_key_der.emplace(public_key_der.value());
  }

  return response;
}

void AuthenticatorCommonImpl::CompleteMakeCredentialRequest(
    blink::mojom::AuthenticatorStatus status,
    blink::mojom::MakeCredentialAuthenticatorResponsePtr response,
    blink::mojom::WebAuthnDOMExceptionDetailsPtr dom_exception_details,
    Focus check_focus) {
  DCHECK(make_credential_response_callback_);
  if (check_focus != Focus::kDontCheck && !(request_delegate_ && IsFocused())) {
    std::move(make_credential_response_callback_)
        .Run(blink::mojom::AuthenticatorStatus::NOT_FOCUSED, nullptr, nullptr);
  } else {
    std::move(make_credential_response_callback_)
        .Run(status, std::move(response), std::move(dom_exception_details));
  }

  Cleanup();
}

blink::mojom::GetAssertionAuthenticatorResponsePtr
AuthenticatorCommonImpl::CreateGetAssertionResponse(
    device::AuthenticatorGetAssertionResponse response_data) {
  auto response = blink::mojom::GetAssertionAuthenticatorResponse::New();
  auto common_info = blink::mojom::CommonCredentialInfo::New();
  common_info->client_data_json.assign(client_data_json_.begin(),
                                       client_data_json_.end());
  common_info->raw_id = response_data.credential->id;
  common_info->id = Base64UrlEncode(common_info->raw_id);
  response->info = std::move(common_info);
  response->info->authenticator_data =
      response_data.authenticator_data.SerializeToByteArray();
  response->signature = response_data.signature;
  response->authenticator_attachment =
      response_data.transport_used
          ? device::AuthenticatorAttachmentFromTransport(
                *response_data.transport_used)
          : device::AuthenticatorAttachment::kAny;
  response_data.user_entity
      ? response->user_handle.emplace(response_data.user_entity->id)
      : response->user_handle.emplace();

  for (RequestExtension ext : requested_extensions_) {
    switch (ext) {
      case RequestExtension::kAppID:
        DCHECK(app_id_);
        response->echo_appid_extension = true;
        if (response_data.authenticator_data.application_parameter() ==
            CreateApplicationParameter(*app_id_)) {
          response->appid_extension = true;
        }
        break;
      case RequestExtension::kPRF: {
        response->echo_prf = true;
        absl::optional<base::span<const uint8_t>> hmac_secret =
            response_data.hmac_secret;
        if (hmac_secret) {
          auto prf_values = blink::mojom::PRFValues::New();
          DCHECK(hmac_secret->size() == 32 || hmac_secret->size() == 64);
          prf_values->first = device::fido_parsing_utils::Materialize(
              hmac_secret->subspan(0, 32));
          if (hmac_secret->size() == 64) {
            prf_values->second = device::fido_parsing_utils::Materialize(
                hmac_secret->subspan(32, 32));
          }
          response->prf_results = std::move(prf_values);
        } else {
          response->prf_not_evaluated = response_data.hmac_secret_not_evaluated;
        }
        break;
      }
      case RequestExtension::kLargeBlobRead:
        response->echo_large_blob = true;
        response->large_blob = response_data.large_blob;
        break;
      case RequestExtension::kLargeBlobWrite:
        response->echo_large_blob = true;
        response->echo_large_blob_written = true;
        response->large_blob_written = response_data.large_blob_written;
        break;
      case RequestExtension::kGetCredBlob: {
        const absl::optional<cbor::Value>& extensions =
            response_data.authenticator_data.extensions();
        if (extensions) {
          const cbor::Value::MapValue& map = extensions->GetMap();
          const auto& it = map.find(cbor::Value(device::kExtensionCredBlob));
          if (it != map.end() && it->second.is_bytestring()) {
            response->get_cred_blob = it->second.GetBytestring();
          }
        }
        if (!response->get_cred_blob.has_value()) {
          // The authenticator is supposed to return an empty byte string if it
          // does not have a credBlob for the credential. But in case it
          // doesn't, we return one to the caller anyway.
          response->get_cred_blob = std::vector<uint8_t>();
        }

        break;
      }
      case RequestExtension::kDevicePublicKey: {
        const absl::optional<cbor::Value>& maybe_extensions =
            response_data.authenticator_data.extensions();
        if (maybe_extensions) {
          DCHECK(maybe_extensions->is_map());
          const cbor::Value::MapValue& extensions = maybe_extensions->GetMap();

          const auto it =
              extensions.find(cbor::Value(device::kExtensionDevicePublicKey));
          if (it != extensions.end() && it->second.is_bytestring()) {
            response->device_public_key =
                blink::mojom::DevicePublicKeyResponse::New();
            response->device_public_key->authenticator_output =
                it->second.GetBytestring();
            response->device_public_key->signature =
                *response_data.device_public_key_signature;
          }
        }
        break;
      }
      case RequestExtension::kHMACSecret:
      case RequestExtension::kCredProps:
      case RequestExtension::kLargeBlobEnable:
      case RequestExtension::kCredBlob:
      case RequestExtension::kMinPINLength:
        NOTREACHED();
        break;
    }
  }

  return response;
}

void AuthenticatorCommonImpl::CompleteGetAssertionRequest(
    blink::mojom::AuthenticatorStatus status,
    blink::mojom::GetAssertionAuthenticatorResponsePtr response,
    blink::mojom::WebAuthnDOMExceptionDetailsPtr dom_exception_details) {
  DCHECK(get_assertion_response_callback_);

  if (status == blink::mojom::AuthenticatorStatus::SUCCESS) {
    static_cast<RenderFrameHostImpl*>(GetRenderFrameHost())
        ->WebAuthnAssertionRequestSucceeded();
  }

  std::move(get_assertion_response_callback_)
      .Run(status, std::move(response), std::move(dom_exception_details));
  Cleanup();
}

void AuthenticatorCommonImpl::Cleanup() {
  timer_->Stop();
  has_pending_request_ = false;
  request_handler_.reset();
  discovery_factory_.reset();
  discovery_factory_testing_override_ = nullptr;
  ctap_make_credential_request_.reset();
  make_credential_options_.reset();
  ctap_get_assertion_request_.reset();
  ctap_get_assertion_options_.reset();
  device_public_key_attestation_requested_ = false;
  awaiting_attestation_response_ = false;
  request_delegate_.reset();
  make_credential_response_callback_.Reset();
  get_assertion_response_callback_.Reset();
  client_data_json_.clear();
  app_id_.reset();
  caller_origin_ = url::Origin();
  relying_party_id_.clear();
  error_awaiting_user_acknowledgement_ =
      blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR;
  requested_extensions_.clear();
  pending_proxied_request_id_.reset();
  discoverable_credential_request_ = false;
}

void AuthenticatorCommonImpl::DisableUI() {
  disable_ui_ = true;
}

void AuthenticatorCommonImpl::DisableTLSCheck() {
  disable_tls_check_ = true;
}

RenderFrameHost* AuthenticatorCommonImpl::GetRenderFrameHost() const {
  RenderFrameHost* ret = RenderFrameHost::FromID(render_frame_host_id_);
  DCHECK(ret);
  return ret;
}

BrowserContext* AuthenticatorCommonImpl::GetBrowserContext() const {
  return GetRenderFrameHost()->GetBrowserContext();
}

device::FidoDiscoveryFactory* AuthenticatorCommonImpl::discovery_factory() {
  DCHECK(discovery_factory_);
  return discovery_factory_testing_override_
             ? discovery_factory_testing_override_.get()
             : discovery_factory_.get();
}

void AuthenticatorCommonImpl::InitDiscoveryFactory() {
  discovery_factory_ = MakeDiscoveryFactory(GetRenderFrameHost());
  // TODO(martinkr): |discovery_factory_testing_override_| is a long-lived
  // VirtualFidoDeviceDiscovery so that tests can maintain and alter virtual
  // authenticator state in between requests. We should extract a longer-lived
  // configuration object from VirtualFidoDeviceDiscovery, so we can simply
  // stick a short-lived instance into |discovery_factory_| and eliminate
  // |discovery_factory_testing_override_|.
  discovery_factory_testing_override_ =
      AuthenticatorEnvironment::GetInstance()
          ->MaybeGetDiscoveryFactoryTestOverride();
}

void AuthenticatorCommonImpl::EnableRequestProxyExtensionsAPISupport() {
  enable_request_proxy_api_ = true;
}

WebAuthenticationRequestProxy*
AuthenticatorCommonImpl::GetWebAuthnRequestProxyIfActive(
    const url::Origin& caller_origin) {
  DCHECK(!caller_origin.opaque());
  // The Virtual Authenticator, which can be activated via Dev Tools UI or
  // ChromeDriver, should take precedence over request proxying. Otherwise
  // attaching a remote desktop session would interfere with automated or manual
  // testing.
  const bool virtual_authenticator_active =
      AuthenticatorEnvironment::GetInstance()
          ->MaybeGetVirtualAuthenticatorManager(
              static_cast<RenderFrameHostImpl*>(GetRenderFrameHost())
                  ->frame_tree_node()) != nullptr;
  if (!enable_request_proxy_api_ || virtual_authenticator_active) {
    return nullptr;
  }
  return GetWebAuthenticationDelegate()->MaybeGetRequestProxy(
      GetBrowserContext(), caller_origin);
}

void AuthenticatorCommonImpl::OnMakeCredentialProxyResponse(
    WebAuthenticationRequestProxy::RequestId request_id,
    blink::mojom::WebAuthnDOMExceptionDetailsPtr error,
    blink::mojom::MakeCredentialAuthenticatorResponsePtr response) {
  DCHECK_EQ(*pending_proxied_request_id_, request_id);
  DCHECK(make_credential_response_callback_);
  pending_proxied_request_id_.reset();
  if (error) {
    DCHECK(!response);
    CompleteMakeCredentialRequest(
        blink::mojom::AuthenticatorStatus::ERROR_WITH_DOM_EXCEPTION_DETAILS,
        nullptr, std::move(error), Focus::kDoCheck);
    return;
  }
  CompleteMakeCredentialRequest(blink::mojom::AuthenticatorStatus::SUCCESS,
                                std::move(response), nullptr, Focus::kDoCheck);
}

void AuthenticatorCommonImpl::OnGetAssertionProxyResponse(
    WebAuthenticationRequestProxy::RequestId request_id,
    blink::mojom::WebAuthnDOMExceptionDetailsPtr error,
    blink::mojom::GetAssertionAuthenticatorResponsePtr response) {
  DCHECK_EQ(*pending_proxied_request_id_, request_id);
  DCHECK(get_assertion_response_callback_);
  pending_proxied_request_id_.reset();
  if (error) {
    DCHECK(!response);
    CompleteGetAssertionRequest(
        blink::mojom::AuthenticatorStatus::ERROR_WITH_DOM_EXCEPTION_DETAILS,
        nullptr, std::move(error));
    return;
  }
  CompleteGetAssertionRequest(blink::mojom::AuthenticatorStatus::SUCCESS,
                              std::move(response));
}

}  // namespace content
