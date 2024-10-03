// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webauth/authenticator_common_impl.h"

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/base64url.h"
#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "base/timer/timer.h"
#include "build/build_config.h"
#include "components/webauthn/json/value_conversions.h"
#include "content/browser/renderer_host/back_forward_cache_disable.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webauth/authenticator_environment.h"
#include "content/browser/webauth/client_data_json.h"
#include "content/browser/webauth/virtual_authenticator.h"
#include "content/browser/webauth/virtual_authenticator_manager_impl.h"
#include "content/browser/webauth/virtual_fido_discovery_factory.h"
#include "content/browser/webauth/webauth_request_security_checker.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "crypto/sha2.h"
#include "device/bluetooth/bluetooth_adapter.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/fido/attestation_statement.h"
#include "device/fido/authenticator_data.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/ctap_make_credential_request.h"
#include "device/fido/features.h"
#include "device/fido/fido_authenticator.h"
#include "device/fido/fido_constants.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_request_handler_base.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/filter.h"
#include "device/fido/get_assertion_request_handler.h"
#include "device/fido/make_credential_request_handler.h"
#include "device/fido/public_key.h"
#include "device/fido/public_key_credential_descriptor.h"
#include "device/fido/public_key_credential_params.h"
#include "net/cert/asn1_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/boringssl/src/include/openssl/sha.h"
#include "third_party/boringssl/src/pki/input.h"
#include "third_party/boringssl/src/pki/parse_values.h"
#include "third_party/boringssl/src/pki/parser.h"

#if BUILDFLAG(IS_MAC)
#include "device/fido/mac/authenticator.h"
#include "device/fido/mac/credential_metadata.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "device/fido/cros/authenticator.h"
#endif

#if BUILDFLAG(IS_WIN)
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
};

enum class AttestationErasureOption {
  kIncludeAttestation,
  kEraseAttestationButIncludeAaguid,
  kEraseAttestationAndAaguid,
};

using GetAssertionOutcome = AuthenticatorCommonImpl::GetAssertionOutcome;
using MakeCredentialOutcome = AuthenticatorCommonImpl::MakeCredentialOutcome;
using RequestMode = AuthenticatorCommonImpl::RequestMode;
using MakeCredentialCallback =
    blink::mojom::Authenticator::MakeCredentialCallback;
using GetAssertionCallback = blink::mojom::Authenticator::GetAssertionCallback;
using ReportCallback = blink::mojom::Authenticator::ReportCallback;

namespace {

WebAuthenticationDelegate* GetWebAuthenticationDelegate() {
  return GetContentClient()->browser()->GetWebAuthenticationDelegate();
}

std::string Base64UrlEncode(const base::span<const uint8_t> input) {
  std::string ret;
  base::Base64UrlEncode(
      std::string_view(reinterpret_cast<const char*>(input.data()),
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
    std::optional<std::string> app_id) {
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
  std::string_view contents;
  if (!net::asn1::ExtractExtensionFromDERCert(
          std::string_view(reinterpret_cast<const char*>(der_cert.data()),
                           der_cert.size()),
          std::string_view(reinterpret_cast<const char*>(kTransportTypesOID),
                           sizeof(kTransportTypesOID)),
          &present, &critical, &contents) ||
      !present) {
    return false;
  }

  const bssl::der::Input contents_der(contents);
  bssl::der::Parser contents_parser(contents_der);
  std::optional<bssl::der::BitString> transport_bits =
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

base::TimeDelta AdjustTimeout(std::optional<base::TimeDelta> timeout,
                              RenderFrameHost* render_frame_host) {
  // Time to wait for an authenticator to successfully complete an operation.
  base::TimeDelta adjusted_timeout_lower = base::Minutes(3);
  base::TimeDelta adjusted_timeout_upper = base::Hours(20);
  if (!timeout) {
    return adjusted_timeout_upper;
  }
  const bool testing_api_enabled =
      AuthenticatorEnvironment::GetInstance()->IsVirtualAuthenticatorEnabledFor(
          static_cast<RenderFrameHostImpl*>(render_frame_host)
              ->frame_tree_node()) ||
      AuthenticatorEnvironment::GetInstance()
          ->MaybeGetDiscoveryFactoryTestOverride();
  if (testing_api_enabled) {
    return *timeout;
  }
  return std::max(adjusted_timeout_lower,
                  std::min(adjusted_timeout_upper, *timeout));
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
    bool uses_discoverable_creds,
    std::optional<bool> is_uvpaa_override) {
  base::flat_set<device::FidoTransportProtocol> transports;
  transports.insert(device::FidoTransportProtocol::kUsbHumanInterfaceDevice);

  // Only instantiate platform discovery if the embedder hasn't chosen to
  // override IsUserVerifyingPlatformAuthenticatorAvailable() to be false.
  // Chrome disables platform authenticators in Guest modes this way.
  if (!is_uvpaa_override || *is_uvpaa_override) {
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
  // There are two possible PIDs the virtual U2F HID device could use, with or
  // without corp protocol functionality.
  constexpr device::VidPid kChromeOsU2fdVidPid{0x18d1, 0x502c};
  constexpr device::VidPid kChromeOsU2fdCorpVidPid{0x18d1, 0x5212};
  discovery_factory->set_hid_ignore_list(
      {kChromeOsU2fdVidPid, kChromeOsU2fdCorpVidPid});
  discovery_factory->set_generate_request_id_callback(
      GetWebAuthenticationDelegate()->GetGenerateRequestIdCallback(
          render_frame_host));
#endif  // BUILDFLAG(IS_CHROMEOS)

  return discovery_factory;
}

std::optional<device::CredProtectRequest> ProtectionPolicyToCredProtect(
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
#if BUILDFLAG(IS_WIN)
      // On Windows, if webauthn.dll is version two or below, rk=preferred
      // cannot be expressed and will be mapped to rk=false. Some security keys
      // have a bug where they'll return credProtect=1 when credProtect=2 is
      // requested for non-discoverable credentials. Thus, for these versions
      // of webauthn.dll, treat rk=preferred as rk=discouraged for the purposes
      // of credProtect, because that's what will ultimately be sent to the
      // security key.
      //
      // If a site explicitly requests a credProtect level, we'll still respect
      // that because they are presumably going to check the response.
      if (base::FeatureList::IsEnabled(
              device::kWebAuthnCredProtectWin10BugWorkaround) &&
          make_credential_options.resident_key ==
              device::ResidentKeyRequirement::kPreferred &&
          device::WinWebAuthnApi::GetDefault() &&
          device::WinWebAuthnApi::GetDefault()->Version() < 3) {
        return std::nullopt;
      }
#endif
      if (make_credential_options.resident_key !=
          device::ResidentKeyRequirement::kDiscouraged) {
        // Otherwise, kUVOrCredIDRequired is made the default unless
        // the authenticator defaults to something better.
        return device::CredProtectRequest::kUVOrCredIDRequiredOrBetter;
      }
      return std::nullopt;
    case blink::mojom::ProtectionPolicy::NONE:
      return device::CredProtectRequest::kUVOptional;
    case blink::mojom::ProtectionPolicy::UV_OR_CRED_ID_REQUIRED:
      return device::CredProtectRequest::kUVOrCredIDRequired;
    case blink::mojom::ProtectionPolicy::UV_REQUIRED:
      return device::CredProtectRequest::kUVRequired;
  }
}

// HashPRFValue hashes a PRF evaluation point with a fixed prefix in order to
// separate the set of points that a website can evaluate. See
// https://w3c.github.io/webauthn/#prf-extension.
std::array<uint8_t, 32> HashPRFValue(base::span<const uint8_t> value) {
  constexpr char kPrefix[] = "WebAuthn PRF";

  SHA256_CTX ctx;
  SHA256_Init(&ctx);
  // This deliberately includes the terminating NUL.
  SHA256_Update(&ctx, kPrefix, sizeof(kPrefix));
  SHA256_Update(&ctx, value.data(), value.size());

  std::array<uint8_t, 32> digest;
  SHA256_Final(digest.data(), &ctx);
  return digest;
}

std::optional<device::PRFInput> ParsePRFInputForMakeCredential(
    const blink::mojom::PRFValuesPtr& prf_input_from_renderer) {
  // The input cannot be credential-specific because we haven't created the
  // credential yet.
  if (prf_input_from_renderer->id) {
    return std::nullopt;
  }

  device::PRFInput prf_input;
  prf_input.salt1 = HashPRFValue(prf_input_from_renderer->first);
  if (prf_input_from_renderer->second) {
    prf_input.salt2 = HashPRFValue(*prf_input_from_renderer->second);
  }

  return prf_input;
}

std::optional<std::vector<device::PRFInput>> ParsePRFInputsForGetAssertion(
    base::span<const blink::mojom::PRFValuesPtr> inputs) {
  std::vector<device::PRFInput> ret;
  bool is_first = true;
  std::optional<std::vector<uint8_t>> last_id;

  // TODO(agl): should match the credential IDs from the allow list, which
  // will also limit the size to the size of the allow list.
  for (const auto& prf_input_from_renderer : inputs) {
    device::PRFInput prf_input;

    // This statement enforces invariants that should be established by the
    // renderer.
    if (
        // Only the first element in the vector may be the default.
        (!is_first && !prf_input_from_renderer->id) ||
        // The PRF inputs must be sorted by credential ID to show that there
        // are no duplicates.
        (last_id.has_value() && prf_input_from_renderer->id.has_value() &&
         *last_id >= *prf_input_from_renderer->id)) {
      return std::nullopt;
    }
    is_first = false;
    last_id = prf_input_from_renderer->id;

    if (prf_input_from_renderer->id) {
      prf_input.credential_id = std::move(*prf_input_from_renderer->id);
    }

    prf_input.salt1 = HashPRFValue(prf_input_from_renderer->first);
    if (prf_input_from_renderer->second) {
      prf_input.salt2 = HashPRFValue(*prf_input_from_renderer->second);
    }

    ret.emplace_back(std::move(prf_input));
  }

  return ret;
}

blink::mojom::PRFValuesPtr PRFResultsToValues(
    base::span<const uint8_t> results) {
  auto prf_values = blink::mojom::PRFValues::New();
  DCHECK(results.size() == 32 || results.size() == 64);
  prf_values->first =
      device::fido_parsing_utils::Materialize(results.first(32u));
  if (results.size() == 64) {
    prf_values->second =
        device::fido_parsing_utils::Materialize(results.subspan(32, 32));
  }

  return prf_values;
}

void SetHints(AuthenticatorRequestClientDelegate* request_delegate,
              const base::flat_set<blink::mojom::Hint>& hints) {
  // The first recognised transport takes priority.
  std::optional<device::FidoTransportProtocol> transport;
  for (const auto hint : hints) {
    switch (hint) {
      case blink::mojom::Hint::SECURITY_KEY:
        transport = transport.value_or(
            device::FidoTransportProtocol::kUsbHumanInterfaceDevice);
        break;
      case blink::mojom::Hint::CLIENT_DEVICE:
        transport =
            transport.value_or(device::FidoTransportProtocol::kInternal);
        break;
      case blink::mojom::Hint::HYBRID:
        transport = transport.value_or(device::FidoTransportProtocol::kHybrid);
        break;
    }
  }

  if (transport) {
    AuthenticatorRequestClientDelegate::Hints delegate_hints;
    delegate_hints.transport = transport;
    request_delegate->SetHints(delegate_hints);
  }
}

bool IsPlatformAuthenticatorForInvalidStateError(
    const device::FidoAuthenticator* authenticator) {
  switch (authenticator->GetType()) {
    case device::AuthenticatorType::kTouchID:
    case device::AuthenticatorType::kChromeOS:
    case device::AuthenticatorType::kICloudKeychain:
    case device::AuthenticatorType::kEnclave:
    case device::AuthenticatorType::kChromeOSPasskeys:
      return true;
    // kWinNative can be a platform authenticator but, in the context where this
    // function is used, Windows returns a specific error when InvalidStateError
    // should be returned. Thus, if it didn't return that, then we shouldn't
    // consider it a platform authenticator.
    case device::AuthenticatorType::kWinNative:
    case device::AuthenticatorType::kOther:
    case device::AuthenticatorType::kPhone:
      return false;
  }
}

AuthenticatorCommonImpl::CredentialRequestResult
CredentialRequestResultFromCode(bool success, device::AuthenticatorType type) {
  switch (type) {
    case device::AuthenticatorType::kChromeOS:
    case device::AuthenticatorType::kChromeOSPasskeys:
      return success ? AuthenticatorCommonImpl::CredentialRequestResult::
                           kChromeOSSuccess
                     : AuthenticatorCommonImpl::CredentialRequestResult::
                           kChromeOSError;
    case device::AuthenticatorType::kEnclave:
      return success ? AuthenticatorCommonImpl::CredentialRequestResult::
                           kEnclaveSuccess
                     : AuthenticatorCommonImpl::CredentialRequestResult::
                           kEnclaveError;
    case device::AuthenticatorType::kICloudKeychain:
      return success ? AuthenticatorCommonImpl::CredentialRequestResult::
                           kICloudKeychainSuccess
                     : AuthenticatorCommonImpl::CredentialRequestResult::
                           kICloudKeychainError;
    case device::AuthenticatorType::kOther:
      return success ? AuthenticatorCommonImpl::CredentialRequestResult::
                           kOtherSuccess
                     : AuthenticatorCommonImpl::CredentialRequestResult::
                           kOtherError;
    case device::AuthenticatorType::kPhone:
      return success ? AuthenticatorCommonImpl::CredentialRequestResult::
                           kPhoneSuccess
                     : AuthenticatorCommonImpl::CredentialRequestResult::
                           kPhoneError;
    case device::AuthenticatorType::kTouchID:
      return success ? AuthenticatorCommonImpl::CredentialRequestResult::
                           kTouchIDSuccess
                     : AuthenticatorCommonImpl::CredentialRequestResult::
                           kTouchIDError;
    case device::AuthenticatorType::kWinNative:
      return success ? AuthenticatorCommonImpl::CredentialRequestResult::
                           kWinNativeSuccess
                     : AuthenticatorCommonImpl::CredentialRequestResult::
                           kWinNativeError;
  }
}

void RecordRegisterOutcomeMetric(std::optional<RequestMode> mode,
                                 ukm::SourceId source_id,
                                 MakeCredentialOutcome outcome) {
  CHECK(mode.has_value());
  CHECK(*mode != RequestMode::kConditional);
  ukm::builders::WebAuthn_RegisterCompletion(source_id)
      .SetRegisterCompletionResult(static_cast<int>(outcome))
      .SetRequestMode(static_cast<int>(*mode))
      .Record(ukm::UkmRecorder::Get());
}

void RecordSignOutcomeMetric(std::optional<RequestMode> mode,
                             ukm::SourceId source_id,
                             GetAssertionOutcome outcome) {
  CHECK(mode.has_value());
  ukm::builders::WebAuthn_SignCompletion(source_id)
      .SetSignCompletionResult(static_cast<int>(outcome))
      .SetRequestMode(static_cast<int>(*mode))
      .Record(ukm::UkmRecorder::Get());
}

blink::mojom::WebAuthnClientCapabilityPtr MakeCapability(std::string name,
                                                         bool available) {
  return blink::mojom::WebAuthnClientCapability::New(std::move(name),
                                                     available);
}

inline bool HasSupportedCapability(
    const std::vector<blink::mojom::WebAuthnClientCapabilityPtr>& capabilities,
    std::string_view capability_name) {
  auto capability_it =
      std::find_if(capabilities.begin(), capabilities.end(),
                   [&capability_name](const auto& capability) {
                     return capability->name == capability_name;
                   });

  CHECK(capability_it != capabilities.end())
      << "Capability " << capability_name << " not found.";
  return (*capability_it)->supported;
}

std::vector<blink::mojom::WebAuthnClientCapabilityPtr> InsertIsPPAACapability(
    std::vector<blink::mojom::WebAuthnClientCapabilityPtr> capabilities) {
  bool isUVPAA = HasSupportedCapability(
      capabilities, client_capabilities::kUserVerifyingPlatformAuthenticator);
  bool hybridTransport = HasSupportedCapability(
      capabilities, client_capabilities::kHybridTransport);

  capabilities.push_back(
      MakeCapability(client_capabilities::kPasskeyPlatformAuthenticator,
                     isUVPAA || hybridTransport));
  return capabilities;
}

void DeleteUnacceptedVirtualAuthenticatorCreds(
    RenderFrameHost* render_frame_host,
    std::string_view relying_party_id,
    base::span<uint8_t> user_id,
    base::span<std::vector<uint8_t>> all_accepted_credentials_ids) {
  FrameTreeNode* frame_tree_node =
      static_cast<RenderFrameHostImpl*>(render_frame_host)->frame_tree_node();
  VirtualAuthenticatorManagerImpl* virtual_authenticator_manager =
      AuthenticatorEnvironment::GetInstance()
          ->MaybeGetVirtualAuthenticatorManager(frame_tree_node);
  if (!virtual_authenticator_manager) {
    return;
  }
  for (VirtualAuthenticator* authenticator :
       virtual_authenticator_manager->GetAuthenticators()) {
    std::vector<std::vector<uint8_t>> credential_ids_to_remove;
    for (const auto& registration : authenticator->registrations()) {
      if (registration.second.user && registration.second.rp &&
          registration.second.rp->id == relying_party_id &&
          registration.second.user->id == user_id &&
          !base::Contains(all_accepted_credentials_ids, registration.first)) {
        credential_ids_to_remove.push_back(registration.first);
      }
    }
    for (const std::vector<uint8_t>& credential_id : credential_ids_to_remove) {
      authenticator->RemoveRegistration(credential_id);
    }
  }
}

void UpdateVirtualAuthenticatorUserCreds(RenderFrameHost* render_frame_host,
                                         std::string_view relying_party_id,
                                         base::span<uint8_t> user_id,
                                         std::string_view name,
                                         std::string_view display_name) {
  FrameTreeNode* frame_tree_node =
      static_cast<RenderFrameHostImpl*>(render_frame_host)->frame_tree_node();
  VirtualAuthenticatorManagerImpl* virtual_authenticator_manager =
      AuthenticatorEnvironment::GetInstance()
          ->MaybeGetVirtualAuthenticatorManager(frame_tree_node);
  if (!virtual_authenticator_manager) {
    return;
  }
  for (VirtualAuthenticator* authenticator :
       virtual_authenticator_manager->GetAuthenticators()) {
    authenticator->UpdateUserDetails(relying_party_id, user_id, name,
                                     display_name);
  }
}

void DeleteVirtualAuthenticatorCreds(
    RenderFrameHost* render_frame_host,
    const std::vector<uint8_t>& passkey_credential_id,
    std::string_view relying_party_id) {
  FrameTreeNode* frame_tree_node =
      static_cast<RenderFrameHostImpl*>(render_frame_host)->frame_tree_node();
  VirtualAuthenticatorManagerImpl* virtual_authenticator_manager =
      AuthenticatorEnvironment::GetInstance()
          ->MaybeGetVirtualAuthenticatorManager(frame_tree_node);
  if (!virtual_authenticator_manager) {
    return;
  }
  for (VirtualAuthenticator* authenticator :
       virtual_authenticator_manager->GetAuthenticators()) {
    for (const auto& registration : authenticator->registrations()) {
      if (registration.second.rp &&
          registration.second.rp->id == relying_party_id &&
          registration.first == passkey_credential_id) {
        authenticator->RemoveRegistration(passkey_credential_id);
        return;
      }
    }
  }
}

}  // namespace

// RequestState contains all state that is specific to a single WebAuthn call.
// Since `AuthenticatorCommonImpl` can service multiple calls, it's important
// that this state be reset after processing each one and collecting it into
// this structure makes that easier to enforce.
struct AuthenticatorCommonImpl::RequestState {
  // Uniquely identifies this request in the scope of its owning
  // `AuthenticatorCommonImpl`.
  RequestKey request_key;

  std::unique_ptr<AuthenticatorRequestClientDelegate> request_delegate;
  std::unique_ptr<device::FidoRequestHandlerBase> request_handler;
  std::unique_ptr<device::FidoDiscoveryFactory> discovery_factory;
  // This dangling raw_ptr occurred in:
  // interactive_ui_tests:
  // WebAuthnDevtoolsAutofillIntegrationTest.SelectAccountWithAllowCredentials
  // https://ci.chromium.org/ui/p/chromium/builders/try/mac-rel/1357012/test-results?q=ExactID%3Aninja%3A%2F%2Fchrome%2Ftest%3Ainteractive_ui_tests%2FWebAuthnDevtoolsAutofillIntegrationTest.SelectAccountWithAllowCredentials+VHash%3A81d118f1ad0b63a6
  raw_ptr<device::FidoDiscoveryFactory,
          FlakyDanglingUntriaged | AcrossTasksDanglingUntriaged>
      discovery_factory_testing_override = nullptr;
  absl::variant<absl::monostate,
                MakeCredentialCallback,
                GetAssertionCallback,
                ReportCallback>
      response_callback;
  std::string client_data_json;
  // conditional_ui_treatment tracks any non-standard conditional UI behaviours
  // that have been requested.
  device::FidoRequestHandlerBase::TransportAvailabilityInfo::
      ConditionalUITreatment conditional_ui_treatment =
          device::FidoRequestHandlerBase::TransportAvailabilityInfo::
              ConditionalUITreatment::kDefault;
  url::Origin caller_origin;
  std::string relying_party_id;
  std::unique_ptr<base::OneShotTimer> timer =
      std::make_unique<base::OneShotTimer>();
  std::optional<std::string> app_id;
  absl::variant<absl::monostate,
                device::CtapMakeCredentialRequest,
                device::CtapGetAssertionRequest>
      ctap_request;
  absl::variant<absl::monostate,
                device::MakeCredentialOptions,
                device::CtapGetAssertionOptions>
      request_options;
  blink::mojom::AuthenticatorStatus error_awaiting_user_acknowledgement =
      blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR;
  bool discoverable_credential_request = false;
  // Indicates whether the current request is a modal WebAuthn call, a
  // conditional UI WebAuthn call, or a payment-related request.
  std::optional<RequestMode> mode;
  // The hints set by the request, if any.
  base::flat_set<blink::mojom::Hint> hints;
  std::optional<CredentialRequestResult> request_result;
  absl::variant<absl::monostate, MakeCredentialOutcome, GetAssertionOutcome>
      request_outcome;

  base::flat_set<RequestExtension> requested_extensions;

  // The request ID of a pending proxied MakeCredential or GetAssertion request.
  std::optional<WebAuthenticationRequestProxy::RequestId>
      pending_proxied_request_id;

  // A pending remote validation of an RP ID.
  std::unique_ptr<WebAuthRequestSecurityChecker::RemoteValidation>
      remote_rp_id_validation;
};

// static
std::unique_ptr<AuthenticatorCommon> AuthenticatorCommon::Create(
    RenderFrameHost* render_frame_host) {
  return std::make_unique<AuthenticatorCommonImpl>(
      render_frame_host,
      AuthenticatorCommonImpl::ServingRequestsFor::kInternalUses);
}

AuthenticatorCommonImpl::AuthenticatorCommonImpl(
    RenderFrameHost* render_frame_host,
    ServingRequestsFor serving_requests_for)
    : render_frame_host_id_(render_frame_host->GetGlobalId()),
      serving_requests_for_(serving_requests_for),
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
  req_state_->request_result.reset();
  InitDiscoveryFactory();

  auto* ctap_make_credential_request =
      &absl::get<device::CtapMakeCredentialRequest>(req_state_->ctap_request);
  auto* make_credential_options =
      &absl::get<device::MakeCredentialOptions>(req_state_->request_options);

  req_state_->request_delegate->ConfigureDiscoveries(
      req_state_->caller_origin, req_state_->relying_party_id, RequestSource(),
      device::FidoRequestType::kMakeCredential,
      make_credential_options->resident_key,
      make_credential_options->user_verification,
      ctap_make_credential_request->user.name,
      base::span<const device::CableDiscoveryData>(),
      browser_passkeys_available_, discovery_factory());
  SetHints(req_state_->request_delegate.get(), req_state_->hints);

  make_credential_options->allow_skipping_pin_touch = allow_skipping_pin_touch;

  base::flat_set<device::FidoTransportProtocol> transports =
      GetWebAuthnTransports(GetRenderFrameHost(), discovery_factory(),
                            UsesDiscoverableCreds(*make_credential_options),
                            is_uvpaa_override_);

  auto platform_discoveries =
      discovery_factory()->IsTestOverride()
          ? std::vector<std::unique_ptr<device::FidoDiscoveryBase>>()
          : req_state_->request_delegate->CreatePlatformDiscoveries();
  req_state_->request_handler =
      std::make_unique<device::MakeCredentialRequestHandler>(
          discovery_factory(), std::move(platform_discoveries), transports,
          *ctap_make_credential_request, *make_credential_options,
          base::BindOnce(&AuthenticatorCommonImpl::OnRegisterResponse,
                         weak_factory_.GetWeakPtr()));

  req_state_->request_delegate->RegisterActionCallbacks(
      base::BindOnce(&AuthenticatorCommonImpl::OnCancelFromUI,
                     weak_factory_.GetWeakPtr()) /* cancel_callback */,
      base::BindRepeating(
          &AuthenticatorCommonImpl::StartMakeCredentialRequest,
          weak_factory_.GetWeakPtr(),
          /*allow_skipping_pin_touch=*/false) /* start_over_callback */,
      base::DoNothing() /* account_preselected_callback */,
      base::BindRepeating(
          &device::FidoRequestHandlerBase::StartAuthenticatorRequest,
          req_state_->request_handler->GetWeakPtr()) /* request_callback */,
      base::BindRepeating(
          &device::FidoRequestHandlerBase::PowerOnBluetoothAdapter,
          req_state_->request_handler
              ->GetWeakPtr()) /* bluetooth_adapter_power_on_callback */,
      base::BindRepeating(
          &device::FidoRequestHandlerBase::RequestBluetoothPermission,
          req_state_->request_handler
              ->GetWeakPtr()) /* request_ble_permission_callback */);
  req_state_->request_handler->set_observer(req_state_->request_delegate.get());
}

void AuthenticatorCommonImpl::StartGetAssertionRequest(
    bool allow_skipping_pin_touch) {
  req_state_->request_result.reset();
  InitDiscoveryFactory();

  base::span<const device::CableDiscoveryData> cable_pairings;
  auto* ctap_get_assertion_request =
      &absl::get<device::CtapGetAssertionRequest>(req_state_->ctap_request);
  auto* ctap_get_assertion_options =
      &absl::get<device::CtapGetAssertionOptions>(req_state_->request_options);
  if (ctap_get_assertion_request->cable_extension && IsFocused()) {
    cable_pairings = *ctap_get_assertion_request->cable_extension;
  }
  req_state_->request_delegate->ConfigureDiscoveries(
      req_state_->caller_origin, req_state_->relying_party_id, RequestSource(),
      device::FidoRequestType::kGetAssertion,
      /*resident_key_requirement=*/std::nullopt,
      ctap_get_assertion_request->user_verification,
      /*user_name=*/std::nullopt, cable_pairings, browser_passkeys_available_,
      discovery_factory());
#if BUILDFLAG(IS_CHROMEOS)
  discovery_factory()->set_get_assertion_request_for_legacy_credential_check(
      *ctap_get_assertion_request);
#endif
  SetHints(req_state_->request_delegate.get(), req_state_->hints);

  base::flat_set<device::FidoTransportProtocol> transports =
      GetWebAuthnTransports(GetRenderFrameHost(), discovery_factory(),
                            UsesDiscoverableCreds(*ctap_get_assertion_request),
                            is_uvpaa_override_);

  auto platform_discoveries =
      discovery_factory()->IsTestOverride()
          ? std::vector<std::unique_ptr<device::FidoDiscoveryBase>>()
          : req_state_->request_delegate->CreatePlatformDiscoveries();
  auto request_handler = std::make_unique<device::GetAssertionRequestHandler>(
      discovery_factory(), std::move(platform_discoveries), transports,
      *ctap_get_assertion_request, *ctap_get_assertion_options,
      allow_skipping_pin_touch,
      base::BindOnce(&AuthenticatorCommonImpl::OnSignResponse,
                     weak_factory_.GetWeakPtr()));
  request_handler->transport_availability_info().conditional_ui_treatment =
      req_state_->conditional_ui_treatment;

  req_state_->request_delegate->RegisterActionCallbacks(
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
              ->GetWeakPtr()) /* bluetooth_adapter_power_on_callback */,
      base::BindRepeating(
          &device::FidoRequestHandlerBase::RequestBluetoothPermission,
          request_handler->GetWeakPtr()) /* request_ble_permission_callback */);

  request_handler->set_observer(req_state_->request_delegate.get());
  req_state_->request_handler = std::move(request_handler);
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
    MakeCredentialCallback callback) {
  base::RecordAction(base::UserMetricsAction("WebAuthn.MakeCredential.Start"));
  callback = base::BindOnce(
      &AuthenticatorCommonImpl::GetMetricsWrappedMakeCredentialCallback,
      weak_factory_.GetWeakPtr(), std::move(callback));

  if (req_state_) {
    std::move(callback).Run(blink::mojom::AuthenticatorStatus::PENDING_REQUEST,
                            nullptr, nullptr);
    return;
  }
  req_state_ = std::make_unique<RequestState>();
  req_state_->request_key = RequestKey(next_request_key_);

  req_state_->response_callback = std::move(callback);
  req_state_->hints.insert(options->hints.begin(), options->hints.end());

  if (options->is_payment_credential_creation) {
    req_state_->mode = RequestMode::kPayment;
  } else {
    req_state_->mode = RequestMode::kModalWebAuthn;
  }

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
    req_state_->request_outcome = MakeCredentialOutcome::kSecurityError;
    CompleteMakeCredentialRequest(status);
    return;
  }

  if (!security_checker_->DeduplicateCredentialDescriptorListAndValidateLength(
          &options->exclude_credentials)) {
    mojo::ReportBadMessage("invalid exclude_credentials length");
    req_state_->request_outcome = MakeCredentialOutcome::kOtherFailure;
    CompleteMakeCredentialRequest(
        blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
    return;
  }

  const std::string relying_party_id = options->relying_party.id;
  const blink::mojom::RemoteDesktopClientOverridePtr&
      remote_desktop_client_override = options->remote_desktop_client_override;
  std::unique_ptr<WebAuthRequestSecurityChecker::RemoteValidation>
      remote_validation = security_checker_->ValidateDomainAndRelyingPartyID(
          caller_origin, relying_party_id, request_type,
          remote_desktop_client_override,
          base::BindOnce(
              &AuthenticatorCommonImpl::ContinueMakeCredentialAfterRpIdCheck,
              weak_factory_.GetWeakPtr(), GetRequestKey(), caller_origin,
              std::move(options), is_cross_origin_iframe));

  // If `remote_validation` is nullptr then the request may already have
  // completed.
  if (remote_validation) {
    req_state_->remote_rp_id_validation = std::move(remote_validation);
  }
}

void AuthenticatorCommonImpl::ContinueMakeCredentialAfterRpIdCheck(
    RequestKey request_key,
    url::Origin caller_origin,
    blink::mojom::PublicKeyCredentialCreationOptionsPtr options,
    bool is_cross_origin_iframe,
    blink::mojom::AuthenticatorStatus rp_id_validation_result) {
  if (!CheckRequestKey(request_key)) {
    return;
  }
  req_state_->remote_rp_id_validation.reset();

  if (rp_id_validation_result != blink::mojom::AuthenticatorStatus::SUCCESS) {
    req_state_->request_outcome = MakeCredentialOutcome::kSecurityError;
    CompleteMakeCredentialRequest(rp_id_validation_result);
    return;
  }

  req_state_->request_delegate = MaybeCreateRequestDelegate();
  if (!req_state_->request_delegate) {
    req_state_->request_outcome = MakeCredentialOutcome::kOtherFailure;
    CompleteMakeCredentialRequest(
        blink::mojom::AuthenticatorStatus::PENDING_REQUEST);
    return;
  }

  if (!req_state_->request_delegate->IsVirtualEnvironmentEnabled() &&
      !disable_tls_check_ &&
      !GetContentClient()->browser()->IsSecurityLevelAcceptableForWebAuthn(
          GetRenderFrameHost(), caller_origin)) {
    req_state_->request_outcome = MakeCredentialOutcome::kOtherFailure;
    CompleteMakeCredentialRequest(
        blink::mojom::AuthenticatorStatus::CERTIFICATE_ERROR);
    return;
  }

  req_state_->caller_origin = caller_origin;
  req_state_->relying_party_id = options->relying_party.id;

  std::optional<std::string> appid_exclude;
  if (options->appid_exclude) {
    appid_exclude = "";
    auto add_id_status = security_checker_->ValidateAppIdExtension(
        *options->appid_exclude, caller_origin,
        options->remote_desktop_client_override, &appid_exclude.value());
    if (add_id_status != blink::mojom::AuthenticatorStatus::SUCCESS) {
      req_state_->request_outcome = MakeCredentialOutcome::kSecurityError;
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
      req_state_->request_outcome = MakeCredentialOutcome::kOtherFailure;
      CompleteMakeCredentialRequest(
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    }
    options->remote_desktop_client_override =
        blink::mojom::RemoteDesktopClientOverride::New(
            /*origin=*/req_state_->caller_origin,
            /*same_origin_with_ancestors=*/!is_cross_origin_iframe);
    req_state_->pending_proxied_request_id = proxy->SignalCreateRequest(
        options,
        base::BindOnce(&AuthenticatorCommonImpl::OnMakeCredentialProxyResponse,
                       weak_factory_.GetWeakPtr(), GetRequestKey()));
    return;
  }

  // Let the embedder override the RP ID to use for the request. In practice
  // this rewrites the RP ID that Chrome extensions use.
  std::optional<std::string> rp_id_override =
      GetWebAuthenticationDelegate()->MaybeGetRelyingPartyIdOverride(
          options->relying_party.id, caller_origin);
  if (rp_id_override) {
    options->relying_party.id = *rp_id_override;
    req_state_->relying_party_id = *rp_id_override;
  }
  req_state_->request_delegate->SetRelyingPartyId(req_state_->relying_party_id);
  req_state_->request_delegate->SetUserEntityForMakeCredentialRequest(
      options->user);

  device::fido_filter::MaybeInitialize();
  switch (device::fido_filter::Evaluate(
      device::fido_filter::Operation::MAKE_CREDENTIAL,
      req_state_->relying_party_id,
      /*device=*/std::nullopt,
      /*id=*/std::nullopt)) {
    case device::fido_filter::Action::ALLOW:
      break;
    case device::fido_filter::Action::NO_ATTESTATION:
      // This will be handled by the request handler.
      break;
    case device::fido_filter::Action::BLOCK:
      req_state_->request_outcome = MakeCredentialOutcome::kFilterBlock;
      CompleteMakeCredentialRequest(
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
  }

  if (!IsFocused()) {
    req_state_->request_outcome = MakeCredentialOutcome::kOtherFailure;
    CompleteMakeCredentialRequest(
        blink::mojom::AuthenticatorStatus::NOT_FOCUSED);
    return;
  }

  const device::AuthenticatorSelectionCriteria
      authenticator_selection_criteria =
          options->authenticator_selection
              ? *options->authenticator_selection
              : device::AuthenticatorSelectionCriteria();

  req_state_->request_options =
      device::MakeCredentialOptions(authenticator_selection_criteria);
  auto* make_credential_options =
      &absl::get<device::MakeCredentialOptions>(req_state_->request_options);
  make_credential_options->json =
      base::MakeRefCounted<device::JSONRequest>(webauthn::ToValue(options));
  const bool might_create_resident_key =
      make_credential_options->resident_key !=
      device::ResidentKeyRequirement::kDiscouraged;
  if (might_create_resident_key &&
      !GetWebAuthenticationDelegate()->SupportsResidentKeys(
          GetRenderFrameHost())) {
    if (make_credential_options->resident_key ==
        device::ResidentKeyRequirement::kRequired) {
      req_state_->request_outcome = MakeCredentialOutcome::kRkNotSupported;
      CompleteMakeCredentialRequest(
          blink::mojom::AuthenticatorStatus::RESIDENT_CREDENTIALS_UNSUPPORTED);
      return;
    }
    // Downgrade 'preferred' to 'discouraged'.
    make_credential_options->resident_key =
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
    req_state_->request_outcome = MakeCredentialOutcome::kOtherFailure;
    CompleteMakeCredentialRequest(
        blink::mojom::AuthenticatorStatus::PROTECTION_POLICY_INCONSISTENT);
    return;
  }

  std::optional<device::CredProtectRequest> cred_protect_request =
      ProtectionPolicyToCredProtect(options->protection_policy,
                                    *make_credential_options);
  if (cred_protect_request) {
    make_credential_options->cred_protect_request = {
        {*cred_protect_request, options->enforce_protection_policy}};
  }

  // Touch-to-Autofill should be proxied without UI.
  if (disable_ui_) {
    req_state_->request_delegate->DisableUI();
  }

  // Assemble clientDataJSON.
  ClientDataJsonParams client_data_json_params(
      ClientDataRequestType::kWebAuthnCreate, req_state_->caller_origin,
      GetRenderFrameHost()->GetOutermostMainFrame()->GetLastCommittedOrigin(),
      options->challenge, is_cross_origin_iframe);
  if (options->remote_desktop_client_override) {
    client_data_json_params.origin =
        options->remote_desktop_client_override->origin;
    client_data_json_params.is_cross_origin_iframe =
        !options->remote_desktop_client_override->same_origin_with_ancestors;
  }
  req_state_->client_data_json =
      BuildClientDataJson(std::move(client_data_json_params));

  req_state_->ctap_request = device::CtapMakeCredentialRequest(
      req_state_->client_data_json, options->relying_party, options->user,
      device::PublicKeyCredentialParams(options->public_key_parameters));
  auto* ctap_make_credential_request =
      &absl::get<device::CtapMakeCredentialRequest>(req_state_->ctap_request);

  ctap_make_credential_request->exclude_list = options->exclude_credentials;
  if (options->prf_enable) {
    req_state_->requested_extensions.insert(RequestExtension::kPRF);
    ctap_make_credential_request->hmac_secret = true;

    if (options->prf_input) {
      std::optional<device::PRFInput> prf_input =
          ParsePRFInputForMakeCredential(options->prf_input);
      if (!prf_input) {
        mojo::ReportBadMessage("invalid PRF inputs");
        req_state_->request_outcome = MakeCredentialOutcome::kOtherFailure;
        CompleteMakeCredentialRequest(
            blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
        return;
      }
      ctap_make_credential_request->prf_input = std::move(*prf_input);
    }
  }
  if (options->hmac_create_secret) {
    req_state_->requested_extensions.insert(RequestExtension::kHMACSecret);
    ctap_make_credential_request->hmac_secret = true;
  }
  if (options->cred_props) {
    req_state_->requested_extensions.insert(RequestExtension::kCredProps);
  }
  if (options->large_blob_enable != device::LargeBlobSupport::kNotRequested) {
    req_state_->requested_extensions.insert(RequestExtension::kLargeBlobEnable);
  }
  if (options->cred_blob) {
    req_state_->requested_extensions.insert(RequestExtension::kCredBlob);
    ctap_make_credential_request->cred_blob = *options->cred_blob;
  }
  if (options->min_pin_length_requested) {
    req_state_->requested_extensions.insert(RequestExtension::kMinPINLength);
    ctap_make_credential_request->min_pin_length_requested = true;
  }
  make_credential_options->large_blob_support = options->large_blob_enable;
  ctap_make_credential_request->app_id_exclude = std::move(appid_exclude);
  make_credential_options->is_off_the_record_context =
      GetBrowserContext()->IsOffTheRecord();

  // Compute the effective attestation conveyance preference.
  device::AttestationConveyancePreference attestation = options->attestation;
  // Enterprise attestation should not have been approved by this point.
  DCHECK_NE(
      attestation,
      device::AttestationConveyancePreference::kEnterpriseApprovedByBrowser);
  if (attestation == device::AttestationConveyancePreference::
                         kEnterpriseIfRPListedOnAuthenticator &&
      GetWebAuthenticationDelegate()->ShouldPermitIndividualAttestation(
          GetBrowserContext(), caller_origin, req_state_->relying_party_id)) {
    attestation =
        device::AttestationConveyancePreference::kEnterpriseApprovedByBrowser;
  }
  ctap_make_credential_request->attestation_preference = attestation;
  GetWebAuthenticationDelegate()->BrowserProvidedPasskeysAvailable(
      GetBrowserContext(),
      base::BindOnce(
          &AuthenticatorCommonImpl::
              ContinueMakeCredentialAfterBrowserPasskeysAvailabilityCheck,
          weak_factory_.GetWeakPtr(), GetRequestKey()));
}

void AuthenticatorCommonImpl::
    ContinueMakeCredentialAfterBrowserPasskeysAvailabilityCheck(
        RequestKey request_key,
        bool available) {
  if (!CheckRequestKey(request_key)) {
    return;
  }
  browser_passkeys_available_ = available;
  GetWebAuthenticationDelegate()
      ->IsUserVerifyingPlatformAuthenticatorAvailableOverride(
          GetRenderFrameHost(),
          base::BindOnce(&AuthenticatorCommonImpl::
                             ContinueMakeCredentialAfterIsUvpaaOverrideCheck,
                         weak_factory_.GetWeakPtr(), GetRequestKey()));
}

void AuthenticatorCommonImpl::ContinueMakeCredentialAfterIsUvpaaOverrideCheck(
    RequestKey request_key,
    std::optional<bool> is_uvpaa_override) {
  if (!CheckRequestKey(request_key)) {
    return;
  }
  is_uvpaa_override_ = is_uvpaa_override;
  StartMakeCredentialRequest(/*allow_skipping_pin_touch=*/true);
}

void AuthenticatorCommonImpl::GetAssertion(
    url::Origin caller_origin,
    blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
    blink::mojom::PaymentOptionsPtr payment_options,
    GetAssertionCallback callback) {
  if (options->is_conditional) {
    base::RecordAction(
        base::UserMetricsAction("WebAuthn.GetAssertion.Conditional.Start"));
  } else {
    base::RecordAction(base::UserMetricsAction("WebAuthn.GetAssertion.Start"));
  }
  callback = base::BindOnce(
      &AuthenticatorCommonImpl::GetMetricsWrappedGetAssertionCallback,
      weak_factory_.GetWeakPtr(), std::move(callback));

  if (req_state_) {
    std::move(callback).Run(blink::mojom::AuthenticatorStatus::PENDING_REQUEST,
                            nullptr, nullptr);
    return;
  }
  req_state_ = std::make_unique<RequestState>();
  req_state_->request_key = RequestKey(next_request_key_);

  req_state_->response_callback = std::move(callback);
  if (!payment_options.is_null()) {
    req_state_->mode = RequestMode::kPayment;
  } else if (options->is_conditional) {
    req_state_->mode = RequestMode::kConditional;
  } else {
    req_state_->mode = RequestMode::kModalWebAuthn;
  }
  req_state_->hints.insert(options->hints.begin(), options->hints.end());

  if (!options->is_conditional) {
    BeginRequestTimeout(options->timeout);
  } else if (options->timeout) {
    // These are magic values that a site can set to experiment with different
    // conditional UI behaviours.
    //
    // TODO(crbug.com/40066138): remove this and everything else from
    // the CL that added it if this is unused by June 2024.
    switch (options->timeout->InMilliseconds()) {
      case 324441:
        req_state_->conditional_ui_treatment =
            device::FidoRequestHandlerBase::TransportAvailabilityInfo::
                ConditionalUITreatment::kDontShowEmptyConditionalUI;
        break;
      case 324442:
        req_state_->conditional_ui_treatment =
            device::FidoRequestHandlerBase::TransportAvailabilityInfo::
                ConditionalUITreatment::kNeverOfferPasskeyFromAnotherDevice;
        break;
    }
  }

  WebAuthRequestSecurityChecker::RequestType request_type =
      payment_options.is_null()
          ? WebAuthRequestSecurityChecker::RequestType::kGetAssertion
          : WebAuthRequestSecurityChecker::RequestType::
                kGetPaymentCredentialAssertion;
  if (!payment_options.is_null() && options->allow_credentials.empty()) {
    req_state_->request_outcome = GetAssertionOutcome::kOtherFailure;
    CompleteGetAssertionRequest(
        blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
    NOTREACHED_IN_MIGRATION();
    return;
  }
  bool is_cross_origin_iframe = false;
  blink::mojom::AuthenticatorStatus status =
      security_checker_->ValidateAncestorOrigins(caller_origin, request_type,
                                                 &is_cross_origin_iframe);
  if (status != blink::mojom::AuthenticatorStatus::SUCCESS) {
    req_state_->request_outcome = GetAssertionOutcome::kSecurityError;
    CompleteGetAssertionRequest(status);
    return;
  }

  if (!security_checker_->DeduplicateCredentialDescriptorListAndValidateLength(
          &options->allow_credentials)) {
    mojo::ReportBadMessage("invalid allow_credentials length");
    req_state_->request_outcome = GetAssertionOutcome::kOtherFailure;
    CompleteGetAssertionRequest(
        blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
    return;
  }

  const std::string relying_party_id = options->relying_party_id;
  const blink::mojom::RemoteDesktopClientOverridePtr&
      remote_desktop_client_override =
          options->extensions->remote_desktop_client_override;
  std::unique_ptr<WebAuthRequestSecurityChecker::RemoteValidation>
      remote_validation = security_checker_->ValidateDomainAndRelyingPartyID(
          caller_origin, relying_party_id, request_type,
          remote_desktop_client_override,
          base::BindOnce(
              &AuthenticatorCommonImpl::ContinueGetAssertionAfterRpIdCheck,
              weak_factory_.GetWeakPtr(), GetRequestKey(), caller_origin,
              std::move(options), std::move(payment_options),
              is_cross_origin_iframe));

  // If `remote_validation` is nullptr then the request may already have
  // completed.
  if (remote_validation) {
    req_state_->remote_rp_id_validation = std::move(remote_validation);
  }
}

void AuthenticatorCommonImpl::ContinueGetAssertionAfterRpIdCheck(
    RequestKey request_key,
    url::Origin caller_origin,
    blink::mojom::PublicKeyCredentialRequestOptionsPtr options,
    blink::mojom::PaymentOptionsPtr payment_options,
    bool is_cross_origin_iframe,
    blink::mojom::AuthenticatorStatus rp_id_validation_result) {
  if (!CheckRequestKey(request_key)) {
    return;
  }
  req_state_->remote_rp_id_validation.reset();

  if (rp_id_validation_result != blink::mojom::AuthenticatorStatus::SUCCESS) {
    req_state_->request_outcome = GetAssertionOutcome::kSecurityError;
    CompleteGetAssertionRequest(rp_id_validation_result);
    return;
  }

  req_state_->request_delegate = MaybeCreateRequestDelegate();
  if (!req_state_->request_delegate) {
    req_state_->request_outcome = GetAssertionOutcome::kOtherFailure;
    CompleteGetAssertionRequest(
        blink::mojom::AuthenticatorStatus::PENDING_REQUEST);
    return;
  }
  if (!req_state_->request_delegate->IsVirtualEnvironmentEnabled() &&
      !disable_tls_check_ &&
      !GetContentClient()->browser()->IsSecurityLevelAcceptableForWebAuthn(
          GetRenderFrameHost(), caller_origin)) {
    req_state_->request_outcome = GetAssertionOutcome::kOtherFailure;
    CompleteGetAssertionRequest(
        blink::mojom::AuthenticatorStatus::CERTIFICATE_ERROR);
    return;
  }

  req_state_->caller_origin = caller_origin;
  req_state_->relying_party_id = options->relying_party_id;

  if (options->extensions->appid) {
    req_state_->requested_extensions.insert(RequestExtension::kAppID);
    std::string app_id;
    auto add_id_status = security_checker_->ValidateAppIdExtension(
        *options->extensions->appid, caller_origin,
        options->extensions->remote_desktop_client_override, &app_id);
    if (add_id_status != blink::mojom::AuthenticatorStatus::SUCCESS) {
      req_state_->request_outcome = GetAssertionOutcome::kSecurityError;
      CompleteGetAssertionRequest(add_id_status);
      return;
    }
    // `ValidateAppidExtension` must have set a value to use. If not, it would
    // be a security bug, so crashing seems appropriate here.
    CHECK(!app_id.empty());
    req_state_->app_id = app_id;
  }

  WebAuthenticationRequestProxy* proxy =
      GetWebAuthnRequestProxyIfActive(caller_origin);
  if (proxy) {
    if (options->is_conditional ||
        (options->extensions->remote_desktop_client_override)) {
      // Don't allow proxying of an already proxied or conditional request.
      req_state_->request_outcome = GetAssertionOutcome::kOtherFailure;
      CompleteGetAssertionRequest(
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    }
    options->extensions->remote_desktop_client_override =
        blink::mojom::RemoteDesktopClientOverride::New(
            /*origin=*/req_state_->caller_origin,
            /*same_origin_with_ancestors=*/!is_cross_origin_iframe);
    req_state_->pending_proxied_request_id = proxy->SignalGetRequest(
        options,
        base::BindOnce(&AuthenticatorCommonImpl::OnGetAssertionProxyResponse,
                       weak_factory_.GetWeakPtr(), GetRequestKey()));
    return;
  }

  // Let the embedder override the RP ID to use for the request. In practice
  // this rewrites the RP ID that Chrome extension use.
  std::optional<std::string> rp_id_override =
      GetWebAuthenticationDelegate()->MaybeGetRelyingPartyIdOverride(
          options->relying_party_id, caller_origin);
  if (rp_id_override) {
    options->relying_party_id = *rp_id_override;
    req_state_->relying_party_id = *rp_id_override;
  }
  req_state_->request_delegate->SetRelyingPartyId(req_state_->relying_party_id);

  // Assemble clientDataJSON.
  ClientDataJsonParams client_data_json_params(
      ClientDataRequestType::kWebAuthnGet, caller_origin,
      GetRenderFrameHost()->GetOutermostMainFrame()->GetLastCommittedOrigin(),
      options->challenge, is_cross_origin_iframe);
  if (payment_options) {
    client_data_json_params.type = ClientDataRequestType::kPaymentGet;
    client_data_json_params.payment_options = std::move(payment_options);
    client_data_json_params.payment_rp = req_state_->relying_party_id;
  } else if (options->extensions->remote_desktop_client_override) {
    client_data_json_params.origin =
        options->extensions->remote_desktop_client_override->origin;
    client_data_json_params.is_cross_origin_iframe =
        !options->extensions->remote_desktop_client_override
             ->same_origin_with_ancestors;
  }
  req_state_->client_data_json =
      BuildClientDataJson(std::move(client_data_json_params));

  device::fido_filter::MaybeInitialize();
  if (device::fido_filter::Evaluate(
          device::fido_filter::Operation::GET_ASSERTION,
          req_state_->relying_party_id,
          /*device=*/std::nullopt,
          /*id=*/std::nullopt) == device::fido_filter::Action::BLOCK) {
    req_state_->request_outcome = GetAssertionOutcome::kFilterBlock;
    CompleteGetAssertionRequest(
        blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
    return;
  }

  if (disable_ui_) {
    DCHECK(!options->is_conditional);
    req_state_->request_delegate->DisableUI();
  }

  req_state_->request_delegate->SetConditionalRequest(options->is_conditional);
  if (options->is_conditional) {
    req_state_->request_delegate->SetAmbientCredentialTypes(
        options->requested_credential_type_flags);
  }

  req_state_->request_delegate->SetCredentialIdFilter(
      options->allow_credentials);
  if (options->is_conditional) {
    // Conditional mediation requests can only be fulfilled by discoverable
    // credentials. The provided allowCredentials list is stripped and will be
    // used to filter returned passkeys
    options->allow_credentials =
        std::vector<device::PublicKeyCredentialDescriptor>();
  }

  if (options->allow_credentials.empty()) {
    if (!GetWebAuthenticationDelegate()->SupportsResidentKeys(
            GetRenderFrameHost())) {
      req_state_->request_outcome = GetAssertionOutcome::kRkNotSupported;
      CompleteGetAssertionRequest(
          blink::mojom::AuthenticatorStatus::RESIDENT_CREDENTIALS_UNSUPPORTED);
      return;
    }
    req_state_->discoverable_credential_request = true;
  }

  if (options->extensions->large_blob_read &&
      options->extensions->large_blob_write) {
    req_state_->request_outcome = GetAssertionOutcome::kOtherFailure;
    CompleteGetAssertionRequest(
        blink::mojom::AuthenticatorStatus::CANNOT_READ_AND_WRITE_LARGE_BLOB);
    return;
  }

  if (options->extensions->large_blob_read) {
    req_state_->requested_extensions.insert(RequestExtension::kLargeBlobRead);
  } else if (options->extensions->large_blob_write) {
    if (options->allow_credentials.size() != 1) {
      req_state_->request_outcome = GetAssertionOutcome::kOtherFailure;
      CompleteGetAssertionRequest(blink::mojom::AuthenticatorStatus::
                                      INVALID_ALLOW_CREDENTIALS_FOR_LARGE_BLOB);
      return;
    }
    req_state_->requested_extensions.insert(RequestExtension::kLargeBlobWrite);
  }

  req_state_->ctap_request = CreateCtapGetAssertionRequest(
      req_state_->client_data_json, options, req_state_->app_id);
  auto* ctap_get_assertion_request =
      &absl::get<device::CtapGetAssertionRequest>(req_state_->ctap_request);

  req_state_->request_options.emplace<device::CtapGetAssertionOptions>();
  auto* ctap_get_assertion_options =
      &absl::get<device::CtapGetAssertionOptions>(req_state_->request_options);
  ctap_get_assertion_options->is_off_the_record_context =
      GetBrowserContext()->IsOffTheRecord();
  ctap_get_assertion_options->json =
      base::MakeRefCounted<device::JSONRequest>(webauthn::ToValue(options));

  if (options->extensions->prf) {
    req_state_->requested_extensions.insert(RequestExtension::kPRF);

    std::optional<std::vector<device::PRFInput>> prf_inputs =
        ParsePRFInputsForGetAssertion(options->extensions->prf_inputs);

    // This should never happen for inputs from the renderer, which should sort
    // the values itself. Additionally, `prf_inputs_hashed` is for hybrid
    // authenticator support on Android.
    if (!prf_inputs || options->extensions->prf_inputs_hashed) {
      mojo::ReportBadMessage("invalid PRF inputs");
      req_state_->request_outcome = GetAssertionOutcome::kOtherFailure;
      CompleteGetAssertionRequest(
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    }
    ctap_get_assertion_options->prf_inputs = std::move(*prf_inputs);
  }

  if (options->extensions->get_cred_blob) {
    req_state_->requested_extensions.insert(RequestExtension::kGetCredBlob);
    ctap_get_assertion_request->get_cred_blob = true;
  }

  ctap_get_assertion_options->large_blob_read =
      options->extensions->large_blob_read;
  ctap_get_assertion_options->large_blob_write =
      options->extensions->large_blob_write;
  GetWebAuthenticationDelegate()->BrowserProvidedPasskeysAvailable(
      GetBrowserContext(),
      base::BindOnce(
          &AuthenticatorCommonImpl::
              ContinueGetAssertionAfterBrowserPasskeysAvailabilityCheck,
          weak_factory_.GetWeakPtr(), GetRequestKey()));
}

void AuthenticatorCommonImpl::
    ContinueGetAssertionAfterBrowserPasskeysAvailabilityCheck(
        RequestKey request_key,
        bool available) {
  if (!CheckRequestKey(request_key)) {
    return;
  }
  browser_passkeys_available_ = available;
  GetWebAuthenticationDelegate()
      ->IsUserVerifyingPlatformAuthenticatorAvailableOverride(
          GetRenderFrameHost(),
          base::BindOnce(&AuthenticatorCommonImpl::
                             ContinueGetAssertionAfterIsUvpaaOverrideCheck,
                         weak_factory_.GetWeakPtr(), GetRequestKey()));
}

void AuthenticatorCommonImpl::ContinueGetAssertionAfterIsUvpaaOverrideCheck(
    RequestKey request_key,
    std::optional<bool> is_uvpaa_override) {
  if (!CheckRequestKey(request_key)) {
    return;
  }
  is_uvpaa_override_ = is_uvpaa_override;
  StartGetAssertionRequest(/*allow_skipping_pin_touch=*/true);
}

void AuthenticatorCommonImpl::GetClientCapabilities(
    url::Origin caller_origin,
    blink::mojom::Authenticator::GetClientCapabilitiesCallback callback) {
  // IsPPAA is computed based on the results of IsUVPAA and HybridTransport.
  auto completion_callback =
      base::BindOnce(&InsertIsPPAACapability).Then(std::move(callback));

  // IMPORTANT: If you add or remove a capability check below (and expect to
  // collect the results of the check with the `BarrierCallback`), update this
  // constant to match the number of `barrier_callback.Run()` calls. Otherwise,
  // the `GetClientCapabilities()` call will crash or timeout.
  constexpr size_t kNumberOfComputedCapabilities = 4;
  auto barrier_callback =
      base::BarrierCallback<blink::mojom::WebAuthnClientCapabilityPtr>(
          kNumberOfComputedCapabilities, std::move(completion_callback));

  barrier_callback.Run(MakeCapability(
      client_capabilities::kRelatedOrigins,
      base::FeatureList::IsEnabled(device::kWebAuthnRelatedOrigin)));

  IsHybridTransportSupported(
      base::BindOnce(&MakeCapability, client_capabilities::kHybridTransport)
          .Then(barrier_callback));

  IsUvpaaAvailableInternal(
      caller_origin,
      base::BindOnce(&MakeCapability,
                     client_capabilities::kUserVerifyingPlatformAuthenticator)
          .Then(barrier_callback),
      /*is_get_client_capabilities_call=*/true);
  IsConditionalMediationAvailable(
      caller_origin,
      base::BindOnce(&MakeCapability, client_capabilities::kConditionalGet)
          .Then(barrier_callback));
}

void AuthenticatorCommonImpl::IsHybridTransportSupported(
    base::OnceCallback<void(bool)> callback) {
  // Similar to Web Bluetooth API (`navigator.bluetooth.getAvailability()`) we
  // want respect the policy and return `false` if the policy is enforced.
  if (!GetRenderFrameHost()->IsFeatureEnabled(
          blink::mojom::PermissionsPolicyFeature::kBluetooth)) {
    std::move(callback).Run(false);
    return;
  }

  if (!device::BluetoothAdapterFactory::Get()->IsLowEnergySupported()) {
    std::move(callback).Run(false);
    return;
  }

  device::BluetoothAdapterFactory::Get()->GetAdapter(
      base::BindOnce([](scoped_refptr<device::BluetoothAdapter> adapter) {
        return adapter && adapter->IsPresent();
      }).Then(std::move(callback)));
}

void AuthenticatorCommonImpl::IsUvpaaAvailableInternal(
    url::Origin caller_origin,
    blink::mojom::Authenticator::
        IsUserVerifyingPlatformAuthenticatorAvailableCallback callback,
    bool is_get_client_capabilities_call) {
  WebAuthenticationRequestProxy* proxy =
      GetWebAuthnRequestProxyIfActive(caller_origin);
  if (proxy) {
    // Note that IsUvpaa requests can interleave with MakeCredential or
    // GetAssertion, and cannot be cancelled. Thus, we do not set
    // `req_state_->pending_proxied_request_id` here.
    proxy->SignalIsUvpaaRequest(std::move(callback));
    return;
  }

  // Check for a delegate override. Chrome overrides IsUVPAA() in Guest mode.
  GetWebAuthenticationDelegate()
      ->IsUserVerifyingPlatformAuthenticatorAvailableOverride(
          GetRenderFrameHost(),
          base::BindOnce(
              &AuthenticatorCommonImpl::ContinueIsUvpaaAfterOverrideCheck,
              weak_factory_.GetWeakPtr(), std::move(callback),
              is_get_client_capabilities_call));
}

void AuthenticatorCommonImpl::IsUserVerifyingPlatformAuthenticatorAvailable(
    url::Origin caller_origin,
    blink::mojom::Authenticator::
        IsUserVerifyingPlatformAuthenticatorAvailableCallback callback) {
  IsUvpaaAvailableInternal(caller_origin, std::move(callback),
                           /*is_get_client_capabilities_call=*/false);
}

void AuthenticatorCommonImpl::ContinueIsUvpaaAfterOverrideCheck(
    blink::mojom::Authenticator::
        IsUserVerifyingPlatformAuthenticatorAvailableCallback callback,
    bool is_get_client_capabilities_call,
    std::optional<bool> is_uvpaa_override) {
  if (is_uvpaa_override) {
    std::move(callback).Run(*is_uvpaa_override);
    return;
  }

  // Record IsUVPAA result in a UMA metric, but only if they're not the
  // WebAuthenticationDelegate override value, so that results from the testing
  // API and disabling in Guest/Off-The-Record profiles aren't counted.
  auto uma_decorated_callback =
      is_get_client_capabilities_call
          ? std::move(callback)
          : base::BindOnce([](bool available) {
              base::UmaHistogramBoolean(
                  "WebAuthentication.IsUVPlatformAuthenticatorAvailable2",
                  available);
              return available;
            }).Then(std::move(callback));

#if BUILDFLAG(IS_MAC)
  IsUVPlatformAuthenticatorAvailable(GetBrowserContext(),
                                     std::move(uma_decorated_callback));
#elif BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
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
  GetWebAuthenticationDelegate()
      ->IsUserVerifyingPlatformAuthenticatorAvailableOverride(
          GetRenderFrameHost(),
          base::BindOnce(
              &AuthenticatorCommonImpl::
                  ContinueIsConditionalMediationAvailableAfterOverrideCheck,
              weak_factory_.GetWeakPtr(), std::move(caller_origin),
              std::move(callback)));
}

void AuthenticatorCommonImpl::
    ContinueIsConditionalMediationAvailableAfterOverrideCheck(
        url::Origin caller_origin,
        blink::mojom::Authenticator::IsConditionalMediationAvailableCallback
            callback,
        std::optional<bool> is_uvpaa_override) {
  if (is_uvpaa_override.has_value()) {
    std::move(callback).Run(*is_uvpaa_override);
    return;
  }
  // Conditional requests cannot be proxied, signal the feature as unavailable.
  if (GetWebAuthnRequestProxyIfActive(caller_origin)) {
    std::move(callback).Run(false);
    return;
  }
  // Passkeys from a phone can be discovered through conditional mediation. To
  // avoid leaking bluetooth or sync status, advertise the feature as available.
  if (GetWebAuthenticationDelegate()->SupportsPasskeyMetadataSyncing()) {
    std::move(callback).Run(true);
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

void AuthenticatorCommonImpl::Report(
    url::Origin caller_origin,
    blink::mojom::PublicKeyCredentialReportOptionsPtr options,
    blink::mojom::Authenticator::ReportCallback callback) {
  if (req_state_) {
    std::move(callback).Run(blink::mojom::AuthenticatorStatus::PENDING_REQUEST,
                            nullptr);
    return;
  }
  req_state_ = std::make_unique<RequestState>();
  req_state_->request_key = RequestKey(next_request_key_);

  req_state_->response_callback = std::move(callback);
  req_state_->caller_origin = std::move(caller_origin);
  req_state_->relying_party_id = options->relying_party_id;

  bool is_cross_origin_iframe = false;
  blink::mojom::AuthenticatorStatus status =
      security_checker_->ValidateAncestorOrigins(
          req_state_->caller_origin,
          WebAuthRequestSecurityChecker::RequestType::kReport,
          &is_cross_origin_iframe);

  // TODO(crbug.com/347727501): Add test for ValidateAncestorOrigins's status.
  if (status != blink::mojom::AuthenticatorStatus::SUCCESS) {
    CompleteReportRequest(status);
    return;
  }
  std::unique_ptr<WebAuthRequestSecurityChecker::RemoteValidation>
      remote_validation = security_checker_->ValidateDomainAndRelyingPartyID(
          req_state_->caller_origin, req_state_->relying_party_id,
          WebAuthRequestSecurityChecker::RequestType::kReport,
          /*remote_desktop_client_override=*/nullptr,
          base::BindOnce(&AuthenticatorCommonImpl::ContinueReportAfterRpIdCheck,
                         weak_factory_.GetWeakPtr(), GetRequestKey(),
                         std::move(options)));

  // TODO(crbug.com/347727501): Add a test to cover the case when
  // remote_validation is not null. If `remote_validation` is nullptr then the
  // request may already have completed.
  if (remote_validation) {
    req_state_->remote_rp_id_validation = std::move(remote_validation);
  }
}

void AuthenticatorCommonImpl::ContinueReportAfterRpIdCheck(
    RequestKey request_key,
    blink::mojom::PublicKeyCredentialReportOptionsPtr options,
    blink::mojom::AuthenticatorStatus rp_id_validation_result) {
  if (!CheckRequestKey(request_key)) {
    return;
  }
  req_state_->remote_rp_id_validation.reset();
  if (rp_id_validation_result != blink::mojom::AuthenticatorStatus::SUCCESS) {
    CompleteReportRequest(rp_id_validation_result);
    return;
  }
  RenderFrameHost* render_frame_host = GetRenderFrameHost();
  if (options->all_accepted_credentials) {
    DeleteUnacceptedVirtualAuthenticatorCreds(
        render_frame_host, req_state_->relying_party_id,
        options->all_accepted_credentials->user_id,
        options->all_accepted_credentials->all_accepted_credentials_ids);
    GetWebAuthenticationDelegate()->DeleteUnacceptedPasskeys(
        WebContents::FromRenderFrameHost(render_frame_host),
        req_state_->relying_party_id,
        options->all_accepted_credentials->user_id,
        options->all_accepted_credentials->all_accepted_credentials_ids);
  } else if (options->current_user_details) {
    UpdateVirtualAuthenticatorUserCreds(
        render_frame_host, req_state_->relying_party_id,
        options->current_user_details->user_id,
        options->current_user_details->name,
        options->current_user_details->display_name);
    GetWebAuthenticationDelegate()->UpdateUserPasskeys(
        WebContents::FromRenderFrameHost(render_frame_host),
        req_state_->caller_origin, req_state_->relying_party_id,
        options->current_user_details->user_id,
        options->current_user_details->name,
        options->current_user_details->display_name);
  } else if (options->unknown_credential_id) {
    DeleteVirtualAuthenticatorCreds(render_frame_host,
                                    *options->unknown_credential_id,
                                    req_state_->relying_party_id);
    GetWebAuthenticationDelegate()->DeletePasskey(
        WebContents::FromRenderFrameHost(render_frame_host),
        *options->unknown_credential_id, req_state_->relying_party_id);
  }
  CompleteReportRequest(blink::mojom::AuthenticatorStatus::SUCCESS, nullptr);
}

void AuthenticatorCommonImpl::GetMetricsWrappedMakeCredentialCallback(
    MakeCredentialCallback original_callback,
    blink::mojom::AuthenticatorStatus status,
    blink::mojom::MakeCredentialAuthenticatorResponsePtr authenticator_response,
    blink::mojom::WebAuthnDOMExceptionDetailsPtr dom_exception_details) {
  if (req_state_ &&
      req_state_->request_result == CredentialRequestResult::kTimeout) {
    base::RecordAction(
        base::UserMetricsAction("WebAuthn.MakeCredential.Timeout"));
  } else if (req_state_ && req_state_->request_result ==
                               CredentialRequestResult::kUserCancelled) {
    base::RecordAction(
        base::UserMetricsAction("WebAuthn.MakeCredential.Cancelled"));
  } else if (status == blink::mojom::AuthenticatorStatus::SUCCESS) {
    base::RecordAction(
        base::UserMetricsAction("WebAuthn.MakeCredential.Success"));
  } else if (status == blink::mojom::AuthenticatorStatus::ABORT_ERROR) {
    base::RecordAction(
        base::UserMetricsAction("WebAuthn.MakeCredential.Aborted"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("WebAuthn.MakeCredential.Failure"));
  }
  std::move(original_callback)
      .Run(status, std::move(authenticator_response),
           std::move(dom_exception_details));
}

void AuthenticatorCommonImpl::GetMetricsWrappedGetAssertionCallback(
    blink::mojom::Authenticator::GetAssertionCallback callback,
    blink::mojom::AuthenticatorStatus status,
    blink::mojom::GetAssertionAuthenticatorResponsePtr authenticator_response,
    blink::mojom::WebAuthnDOMExceptionDetailsPtr dom_exception_details) {
  if (req_state_ &&
      req_state_->request_result == CredentialRequestResult::kTimeout) {
    base::RecordAction(
        base::UserMetricsAction("WebAuthn.GetAssertion.Timeout"));
  } else if (req_state_ && req_state_->request_result ==
                               CredentialRequestResult::kUserCancelled) {
    base::RecordAction(
        base::UserMetricsAction("WebAuthn.GetAssertion.Cancelled"));
  } else if (status == blink::mojom::AuthenticatorStatus::SUCCESS) {
    base::RecordAction(
        base::UserMetricsAction("WebAuthn.GetAssertion.Success"));
  } else if (status == blink::mojom::AuthenticatorStatus::ABORT_ERROR) {
    base::RecordAction(
        base::UserMetricsAction("WebAuthn.GetAssertion.Aborted"));
  } else {
    base::RecordAction(
        base::UserMetricsAction("WebAuthn.GetAssertion.Failure"));
  }
  std::move(callback).Run(status, std::move(authenticator_response),
                          std::move(dom_exception_details));
}

void AuthenticatorCommonImpl::Cancel() {
  CancelWithStatus(blink::mojom::AuthenticatorStatus::ABORT_ERROR);
}

void AuthenticatorCommonImpl::OnRegisterResponse(
    device::MakeCredentialStatus status_code,
    std::optional<device::AuthenticatorMakeCredentialResponse> response_data,
    const device::FidoAuthenticator* authenticator) {
  CHECK(authenticator);

  if (!req_state_->request_handler) {
    // Either the callback was called immediately and
    // |req_state_->request_handler| has not yet been assigned (this is a bug),
    // or a navigation caused the request to be canceled while a callback was
    // enqueued.
    return;
  }

  req_state_->request_result = CredentialRequestResultFromCode(
      status_code == device::MakeCredentialStatus::kSuccess,
      authenticator->GetType());

  switch (status_code) {
    case device::MakeCredentialStatus::kUserConsentButCredentialExcluded:
    case device::MakeCredentialStatus::kWinInvalidStateError:
      // Duplicate registration: the new credential would be created on an
      // authenticator that already contains one of the credentials in
      // |exclude_credentials|. If the target was a platform authenticator then
      // the RP learns of the result via the distinctive InvalidStateError
      // result. This tells them that the platform authenticator is already
      // registered with one of the credential IDs that they already know about.
      //
      // Windows already behaves like this and so its representation of
      // InvalidStateError is handled this way too.
      req_state_->request_outcome = MakeCredentialOutcome::kCredentialExcluded;
      if ((authenticator &&
           IsPlatformAuthenticatorForInvalidStateError(authenticator)) ||
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
      req_state_->request_outcome =
          MakeCredentialOutcome::kUnknownResponseFromAuthenticator;
      // The response from the authenticator was corrupted.
      CompleteMakeCredentialRequest(
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR, nullptr,
          nullptr, Focus::kDoCheck);
      return;
    case device::MakeCredentialStatus::kHybridTransportError:
      req_state_->request_outcome =
          MakeCredentialOutcome::kHybridTransportError;
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kHybridTransportError,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::MakeCredentialStatus::kEnclaveError:
      req_state_->request_outcome = MakeCredentialOutcome::kEnclaveError;
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kEnclaveError,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::MakeCredentialStatus::kUserConsentDenied:
      req_state_->request_outcome = MakeCredentialOutcome::kUserCancellation;
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kUserConsentDenied,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::MakeCredentialStatus::kSoftPINBlock:
      req_state_->request_outcome = MakeCredentialOutcome::kSoftPinBlock;
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kSoftPINBlock,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::MakeCredentialStatus::kHardPINBlock:
      req_state_->request_outcome = MakeCredentialOutcome::kHardPinBlock;
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kHardPINBlock,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::MakeCredentialStatus::kAuthenticatorRemovedDuringPINEntry:
      req_state_->request_outcome = MakeCredentialOutcome::kOtherFailure;
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kAuthenticatorRemovedDuringPINEntry,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::MakeCredentialStatus::kAuthenticatorMissingResidentKeys:
      req_state_->request_outcome = MakeCredentialOutcome::kRkNotSupported;
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kAuthenticatorMissingResidentKeys,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::MakeCredentialStatus::kAuthenticatorMissingUserVerification:
      req_state_->request_outcome = MakeCredentialOutcome::kUvNotSupported;
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kAuthenticatorMissingUserVerification,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::MakeCredentialStatus::kAuthenticatorMissingLargeBlob:
      req_state_->request_outcome =
          MakeCredentialOutcome::kLargeBlobNotSupported;
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kAuthenticatorMissingLargeBlob,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::MakeCredentialStatus::kNoCommonAlgorithms:
      req_state_->request_outcome =
          MakeCredentialOutcome::kAlgorithmNotSupported;
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kNoCommonAlgorithms,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::MakeCredentialStatus::kStorageFull:
      req_state_->request_outcome = MakeCredentialOutcome::kStorageFull;
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kStorageFull,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::MakeCredentialStatus::kWinNotAllowedError:
      req_state_->request_outcome = MakeCredentialOutcome::kPlatformNotAllowed;
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kWinUserCancelled,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::MakeCredentialStatus::kEnclaveCancel:
      req_state_->request_outcome = MakeCredentialOutcome::kUserCancellation;
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kEnclaveCancel,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::MakeCredentialStatus::kSuccess:
      break;
  }

  DCHECK(response_data.has_value());
  DCHECK(authenticator);

  req_state_->request_delegate->OnTransactionSuccessful(
      RequestSource(), device::FidoRequestType::kMakeCredential,
      authenticator->GetType());

  std::optional<device::FidoTransportProtocol> transport =
      authenticator->AuthenticatorTransport();
  bool is_transport_used_internal = false;
  bool is_transport_used_cable = false;
  if (transport) {
    is_transport_used_internal =
        *transport == device::FidoTransportProtocol::kInternal;
    is_transport_used_cable =
        *transport == device::FidoTransportProtocol::kHybrid;
  }

  const auto attestation =
      absl::get<device::CtapMakeCredentialRequest>(req_state_->ctap_request)
          .attestation_preference;
  std::optional<AttestationErasureOption> attestation_erasure;

  if (response_data->attestation_should_be_filtered &&
      !GetWebAuthenticationDelegate()->ShouldPermitIndividualAttestation(
          GetBrowserContext(), req_state_->caller_origin,
          req_state_->relying_party_id)) {
    attestation_erasure = AttestationErasureOption::kEraseAttestationAndAaguid;
  } else if (response_data->attestation_object
                 .IsAttestationCertificateInappropriatelyIdentifying() &&
             !GetWebAuthenticationDelegate()->ShouldPermitIndividualAttestation(
                 GetBrowserContext(), req_state_->caller_origin,
                 req_state_->relying_party_id)) {
    // If the RP sees a "none" attestation with a zero AAGUID after requesting
    // "direct" attestation then they can reasonably conclude that it was one of
    // the tokens with inappropriate certs. But this is better than disclosing
    // the certificate itself, and these tokens are vanishingly rare now.
    attestation_erasure = AttestationErasureOption::kEraseAttestationAndAaguid;
  } else if (attestation == device::AttestationConveyancePreference::
                                kEnterpriseApprovedByBrowser) {
    // If enterprise attestation was approved by policy then it can be
    // returned immediately.
    attestation_erasure = AttestationErasureOption::kIncludeAttestation;
  } else if (response_data->attestation_object.IsSelfAttestation()) {
    // Self attestation is just a self-signature and carries no identifying
    // information.
    attestation_erasure = AttestationErasureOption::kIncludeAttestation;
  } else if (is_transport_used_internal || is_transport_used_cable) {
    // Direct attestation from platform authenticators is known to be
    // privacy preserving, so we always return it when requested. We follow the
    // same rule when the platform authenticator is used over hybrid so that
    // sites see a consistent experience between syncing and using the phone.
    // Also, counter to what the WebAuthn spec says, we do not erase the AAGUID
    // even when attestation wasn't requested.
    attestation_erasure =
        attestation != device::AttestationConveyancePreference::kNone
            ? AttestationErasureOption::kIncludeAttestation
            : AttestationErasureOption::kEraseAttestationButIncludeAaguid;
  } else if (attestation == device::AttestationConveyancePreference::kNone) {
    attestation_erasure = AttestationErasureOption::kEraseAttestationAndAaguid;
  } else {
    // The UI will have shown a notification that attestation was requested.
    attestation_erasure = AttestationErasureOption::kIncludeAttestation;
  }

  CompleteMakeCredentialRequest(
      blink::mojom::AuthenticatorStatus::SUCCESS,
      CreateMakeCredentialResponse(std::move(*response_data),
                                   *attestation_erasure),
      nullptr, Focus::kDoCheck);
}

void AuthenticatorCommonImpl::OnSignResponse(
    device::GetAssertionStatus status_code,
    std::optional<std::vector<device::AuthenticatorGetAssertionResponse>>
        response_data,
    device::FidoAuthenticator* authenticator) {
  CHECK(authenticator);
  DCHECK(!response_data || !response_data->empty());  // empty vector is invalid

  if (!req_state_->request_handler) {
    // Either the callback was called immediately and
    // |req_state_->request_handler| has not yet been assigned (this is a bug),
    // or a navigation caused the request to be canceled while a callback was
    // enqueued.
    return;
  }

  req_state_->request_result = CredentialRequestResultFromCode(
      status_code == device::GetAssertionStatus::kSuccess,
      authenticator->GetType());

  switch (status_code) {
    case device::GetAssertionStatus::kUserConsentButCredentialNotRecognized:
      req_state_->request_outcome =
          GetAssertionOutcome::kCredentialNotRecognized;
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kKeyNotRegistered,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::GetAssertionStatus::kAuthenticatorResponseInvalid:
      req_state_->request_outcome =
          GetAssertionOutcome::kUnknownResponseFromAuthenticator;
      // The response from the authenticator was corrupted.
      CompleteGetAssertionRequest(
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::GetAssertionStatus::kUserConsentDenied:
      req_state_->request_outcome = GetAssertionOutcome::kUserCancellation;
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kUserConsentDenied,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::GetAssertionStatus::kSoftPINBlock:
      req_state_->request_outcome = GetAssertionOutcome::kSoftPinBlock;
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kSoftPINBlock,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::GetAssertionStatus::kHardPINBlock:
      req_state_->request_outcome = GetAssertionOutcome::kHardPinBlock;
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kHardPINBlock,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::GetAssertionStatus::kAuthenticatorRemovedDuringPINEntry:
      req_state_->request_outcome = GetAssertionOutcome::kOtherFailure;
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kAuthenticatorRemovedDuringPINEntry,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::GetAssertionStatus::kAuthenticatorMissingResidentKeys:
      req_state_->request_outcome = GetAssertionOutcome::kRkNotSupported;
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kAuthenticatorMissingResidentKeys,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::GetAssertionStatus::kAuthenticatorMissingUserVerification:
      req_state_->request_outcome = GetAssertionOutcome::kUvNotSupported;
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kAuthenticatorMissingUserVerification,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::GetAssertionStatus::kWinNotAllowedError:
      req_state_->request_outcome = GetAssertionOutcome::kPlatformNotAllowed;
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kWinUserCancelled,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::GetAssertionStatus::kHybridTransportError:
      req_state_->request_outcome = GetAssertionOutcome::kHybridTransportError;
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kHybridTransportError,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::GetAssertionStatus::kICloudKeychainNoCredentials:
      req_state_->request_outcome =
          GetAssertionOutcome::kCredentialNotRecognized;
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kNoPasskeys,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::GetAssertionStatus::kEnclaveError:
      req_state_->request_outcome = GetAssertionOutcome::kEnclaveError;
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kEnclaveError,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::GetAssertionStatus::kEnclaveCancel:
      req_state_->request_outcome = GetAssertionOutcome::kUserCancellation;
      SignalFailureToRequestDelegate(
          AuthenticatorRequestClientDelegate::InterestingFailureReason::
              kEnclaveCancel,
          blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
      return;
    case device::GetAssertionStatus::kSuccess:
      break;
  }

  DCHECK_EQ(status_code, device::GetAssertionStatus::kSuccess);
  DCHECK(response_data.has_value());

  req_state_->request_delegate->OnTransactionSuccessful(
      RequestSource(), device::FidoRequestType::kGetAssertion,
      authenticator->GetType());

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
    if (!req_state_->discoverable_credential_request ||
        response.user_selected || !response.user_entity ||
        !response.user_entity->name || !response.user_entity->display_name) {
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
  req_state_->request_delegate->SelectAccount(
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
  req_state_->error_awaiting_user_acknowledgement = status;

  // The UI may decide to end the request immediately, or after user
  // confirmation. Either way stop discoveries and authenticators now.
  if (req_state_->request_handler) {
    req_state_->request_handler->StopDiscoveries();
    req_state_->request_handler->CancelActiveAuthenticators();
  }

  if (req_state_->request_delegate->DoesBlockRequestOnFailure(reason)) {
    // The UI may have decided to start the request over. Thus do not assume
    // anything about the state here.
    return;
  }

  // The UI wishes the end the request immediately.
  CancelWithStatus(req_state_->error_awaiting_user_acknowledgement);
}

void AuthenticatorCommonImpl::BeginRequestTimeout(
    std::optional<base::TimeDelta> timeout) {
  req_state_->timer->Start(FROM_HERE,
                           AdjustTimeout(timeout, GetRenderFrameHost()),
                           base::BindOnce(&AuthenticatorCommonImpl::OnTimeout,
                                          weak_factory_.GetWeakPtr()));
}

// TODO(crbug.com/41371792): Add web tests to verify timeouts are
// indistinguishable from NOT_ALLOWED_ERROR cases.
void AuthenticatorCommonImpl::OnTimeout() {
  if (!req_state_->request_delegate) {
    // If no UI has been shown yet (likely because we timed out waiting for RP
    // ID validation) then simply cancel the request.
    CancelWithStatus(blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
    return;
  }

  req_state_->request_result = CredentialRequestResult::kTimeout;
  if (absl::holds_alternative<GetAssertionCallback>(
          req_state_->response_callback)) {
    req_state_->request_outcome = GetAssertionOutcome::kUiTimeout;
  } else {
    req_state_->request_outcome = MakeCredentialOutcome::kUiTimeout;
  }
  SignalFailureToRequestDelegate(
      AuthenticatorRequestClientDelegate::InterestingFailureReason::kTimeout,
      blink::mojom::AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

void AuthenticatorCommonImpl::CancelWithStatus(
    blink::mojom::AuthenticatorStatus status) {
  // Callers may attempt to cancel whether there is a request or not.
  if (!req_state_) {
    return;
  }
  if (req_state_->pending_proxied_request_id) {
    WebAuthenticationRequestProxy* proxy =
        GetWebAuthenticationDelegate()->MaybeGetRequestProxy(
            GetBrowserContext(), req_state_->caller_origin);
    // As long as `req_state_->pending_proxied_request_id` is set, there should
    // be an active request proxy. Deactivation of the proxy would have invoked
    // `OnMakeCredentialProxyResponse()` or `OnGetAssertionProxyResponse()`, and
    // cleared `req_state_->pending_proxied_request_id`
    DCHECK(proxy);
    proxy->CancelRequest(*req_state_->pending_proxied_request_id);
  }

  DCHECK(
      !absl::holds_alternative<absl::monostate>(req_state_->response_callback));
  if (absl::holds_alternative<MakeCredentialCallback>(
          req_state_->response_callback) &&
      absl::get<MakeCredentialCallback>(req_state_->response_callback)) {
    CompleteMakeCredentialRequest(status);
  } else if (absl::holds_alternative<GetAssertionCallback>(
                 req_state_->response_callback) &&
             absl::get<GetAssertionCallback>(req_state_->response_callback)) {
    CompleteGetAssertionRequest(status);
  }
}

void AuthenticatorCommonImpl::OnCancelFromUI() {
  if (!req_state_->request_result &&
      absl::holds_alternative<GetAssertionCallback>(
          req_state_->response_callback) &&
      absl::get<GetAssertionCallback>(req_state_->response_callback)) {
    // The user cancelled before the request finished.
    req_state_->request_result = CredentialRequestResult::kUserCancelled;
    req_state_->request_outcome = GetAssertionOutcome::kUserCancellation;
  } else if (!req_state_->request_result &&
             absl::holds_alternative<MakeCredentialCallback>(
                 req_state_->response_callback) &&
             absl::get<MakeCredentialCallback>(req_state_->response_callback)) {
    req_state_->request_result = CredentialRequestResult::kUserCancelled;
    req_state_->request_outcome = MakeCredentialOutcome::kUserCancellation;
  }
  CancelWithStatus(req_state_->error_awaiting_user_acknowledgement);
}

blink::mojom::MakeCredentialAuthenticatorResponsePtr
AuthenticatorCommonImpl::CreateMakeCredentialResponse(
    device::AuthenticatorMakeCredentialResponse response_data,
    AttestationErasureOption attestation_erasure) {
  auto response = blink::mojom::MakeCredentialAuthenticatorResponse::New();
  auto common_info = blink::mojom::CommonCredentialInfo::New();
  common_info->client_data_json.assign(req_state_->client_data_json.begin(),
                                       req_state_->client_data_json.end());
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
  std::optional<base::span<const uint8_t>> leaf_cert =
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

  switch (attestation_erasure) {
    case AttestationErasureOption::kIncludeAttestation:
      break;
    case AttestationErasureOption::kEraseAttestationButIncludeAaguid:
      response_data.attestation_object.EraseAttestationStatement(
          device::AttestationObject::AAGUID::kInclude);
      break;
    case AttestationErasureOption::kEraseAttestationAndAaguid:
      response_data.attestation_object.EraseAttestationStatement(
          device::AttestationObject::AAGUID::kErase);
      break;
  }

  bool did_create_hmac_secret = response_data.prf_enabled;
  bool did_store_cred_blob = false;
  const std::optional<cbor::Value>& maybe_extensions =
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
  }

  for (const RequestExtension ext : req_state_->requested_extensions) {
    switch (ext) {
      case RequestExtension::kPRF:
        response->echo_prf = true;
        response->prf = did_create_hmac_secret;
        if (response_data.prf_results) {
          response->prf_results =
              PRFResultsToValues(*response_data.prf_results);
        }
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
      case RequestExtension::kAppID:
      case RequestExtension::kLargeBlobRead:
      case RequestExtension::kLargeBlobWrite:
      case RequestExtension::kGetCredBlob:
        NOTREACHED_IN_MIGRATION();
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
  const std::optional<std::vector<uint8_t>>& public_key_der =
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
  DCHECK(absl::holds_alternative<MakeCredentialCallback>(
             req_state_->response_callback) &&
         absl::get<MakeCredentialCallback>(req_state_->response_callback));
  auto make_credential_response_callback = std::move(
      absl::get<MakeCredentialCallback>(req_state_->response_callback));

  if (req_state_->request_result) {
    UMA_HISTOGRAM_ENUMERATION("WebAuthentication.MakeCredential.Result",
                              *req_state_->request_result);
  }

  if (absl::holds_alternative<MakeCredentialOutcome>(
          req_state_->request_outcome)) {
    RecordRegisterOutcomeMetric(
        req_state_->mode, GetRenderFrameHost()->GetPageUkmSourceId(),
        absl::get<MakeCredentialOutcome>(req_state_->request_outcome));
  } else if (status == blink::mojom::AuthenticatorStatus::SUCCESS) {
    RecordRegisterOutcomeMetric(req_state_->mode,
                                GetRenderFrameHost()->GetPageUkmSourceId(),
                                MakeCredentialOutcome::kSuccess);
  }

  if (check_focus != Focus::kDontCheck &&
      !(req_state_->request_delegate && IsFocused())) {
    std::move(make_credential_response_callback)
        .Run(blink::mojom::AuthenticatorStatus::NOT_FOCUSED, nullptr, nullptr);
  } else {
    std::move(make_credential_response_callback)
        .Run(status, std::move(response), std::move(dom_exception_details));
  }

  Cleanup();
}

blink::mojom::GetAssertionAuthenticatorResponsePtr
AuthenticatorCommonImpl::CreateGetAssertionResponse(
    device::AuthenticatorGetAssertionResponse response_data) {
  auto response = blink::mojom::GetAssertionAuthenticatorResponse::New();
  auto common_info = blink::mojom::CommonCredentialInfo::New();
  auto response_extensions =
      blink::mojom::AuthenticationExtensionsClientOutputs::New();
  common_info->client_data_json.assign(req_state_->client_data_json.begin(),
                                       req_state_->client_data_json.end());
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

  for (RequestExtension ext : req_state_->requested_extensions) {
    switch (ext) {
      case RequestExtension::kAppID:
        DCHECK(req_state_->app_id);
        response_extensions->echo_appid_extension = true;
        if (response_data.authenticator_data.application_parameter() ==
            CreateApplicationParameter(*req_state_->app_id)) {
          response_extensions->appid_extension = true;
        }
        break;
      case RequestExtension::kPRF: {
        response_extensions->echo_prf = true;
        if (response_data.hmac_secret) {
          response_extensions->prf_results =
              PRFResultsToValues(*response_data.hmac_secret);
        } else {
          response_extensions->prf_not_evaluated =
              response_data.hmac_secret_not_evaluated;
        }
        break;
      }
      case RequestExtension::kLargeBlobRead:
        response_extensions->echo_large_blob = true;
        response_extensions->large_blob = response_data.large_blob;
        break;
      case RequestExtension::kLargeBlobWrite:
        response_extensions->echo_large_blob = true;
        response_extensions->echo_large_blob_written = true;
        response_extensions->large_blob_written =
            response_data.large_blob_written;
        break;
      case RequestExtension::kGetCredBlob: {
        const std::optional<cbor::Value>& extensions =
            response_data.authenticator_data.extensions();
        if (extensions) {
          const cbor::Value::MapValue& map = extensions->GetMap();
          const auto& it = map.find(cbor::Value(device::kExtensionCredBlob));
          if (it != map.end() && it->second.is_bytestring()) {
            response_extensions->get_cred_blob = it->second.GetBytestring();
          }
        }
        if (!response_extensions->get_cred_blob.has_value()) {
          // The authenticator is supposed to return an empty byte string if it
          // does not have a credBlob for the credential. But in case it
          // doesn't, we return one to the caller anyway.
          response_extensions->get_cred_blob = std::vector<uint8_t>();
        }

        break;
      }
      case RequestExtension::kHMACSecret:
      case RequestExtension::kCredProps:
      case RequestExtension::kLargeBlobEnable:
      case RequestExtension::kCredBlob:
      case RequestExtension::kMinPINLength:
        NOTREACHED_IN_MIGRATION();
        break;
    }
  }
  response->extensions = std::move(response_extensions);

  return response;
}

void AuthenticatorCommonImpl::CompleteGetAssertionRequest(
    blink::mojom::AuthenticatorStatus status,
    blink::mojom::GetAssertionAuthenticatorResponsePtr response,
    blink::mojom::WebAuthnDOMExceptionDetailsPtr dom_exception_details) {
  DCHECK(absl::holds_alternative<GetAssertionCallback>(
             req_state_->response_callback) &&
         absl::get<GetAssertionCallback>(req_state_->response_callback));
  auto get_assertion_response_callback =
      std::move(absl::get<GetAssertionCallback>(req_state_->response_callback));

  if (req_state_->request_result) {
    UMA_HISTOGRAM_ENUMERATION("WebAuthentication.GetAssertion.Result",
                              *req_state_->request_result);
  }

  if (absl::holds_alternative<GetAssertionOutcome>(
          req_state_->request_outcome)) {
    RecordSignOutcomeMetric(
        req_state_->mode, GetRenderFrameHost()->GetPageUkmSourceId(),
        absl::get<GetAssertionOutcome>(req_state_->request_outcome));
  } else if (status == blink::mojom::AuthenticatorStatus::SUCCESS) {
    RecordSignOutcomeMetric(req_state_->mode,
                            GetRenderFrameHost()->GetPageUkmSourceId(),
                            GetAssertionOutcome::kSuccess);
  }

  if (status == blink::mojom::AuthenticatorStatus::SUCCESS) {
    static_cast<RenderFrameHostImpl*>(GetRenderFrameHost())
        ->WebAuthnAssertionRequestSucceeded();
  }

  std::move(get_assertion_response_callback)
      .Run(status, std::move(response), std::move(dom_exception_details));
  Cleanup();
}

void AuthenticatorCommonImpl::CompleteReportRequest(
    blink::mojom::AuthenticatorStatus status,
    blink::mojom::WebAuthnDOMExceptionDetailsPtr dom_exception_details) {
  DCHECK(
      absl::holds_alternative<ReportCallback>(req_state_->response_callback) &&
      absl::get<ReportCallback>(req_state_->response_callback));
  std::move(absl::get<ReportCallback>(req_state_->response_callback))
      .Run(status, std::move(dom_exception_details));
  Cleanup();
}

void AuthenticatorCommonImpl::Cleanup() {
  CHECK(!req_state_ || req_state_->request_key.value() == next_request_key_);
  req_state_.reset();
  next_request_key_++;
  CHECK(next_request_key_);  // crash on overflow. Only 2^64 WebAuthn requests
                             // per instance of this object are supported.
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

AuthenticatorRequestClientDelegate::RequestSource
AuthenticatorCommonImpl::RequestSource() const {
  if (serving_requests_for_ == ServingRequestsFor::kInternalUses) {
    return AuthenticatorRequestClientDelegate::RequestSource::kInternal;
  }
  if (req_state_->mode == RequestMode::kPayment) {
    return AuthenticatorRequestClientDelegate::RequestSource::
        kSecurePaymentConfirmation;
  }
  return AuthenticatorRequestClientDelegate::RequestSource::kWebAuthentication;
}

BrowserContext* AuthenticatorCommonImpl::GetBrowserContext() const {
  return GetRenderFrameHost()->GetBrowserContext();
}

device::FidoDiscoveryFactory* AuthenticatorCommonImpl::discovery_factory() {
  DCHECK(req_state_->discovery_factory);
  return req_state_->discovery_factory_testing_override
             ? req_state_->discovery_factory_testing_override.get()
             : req_state_->discovery_factory.get();
}

void AuthenticatorCommonImpl::InitDiscoveryFactory() {
  req_state_->discovery_factory = MakeDiscoveryFactory(GetRenderFrameHost());
  // TODO(martinkr): |discovery_factory_testing_override_| is a long-lived
  // VirtualFidoDeviceDiscovery so that tests can maintain and alter virtual
  // authenticator state in between requests. We should extract a longer-lived
  // configuration object from VirtualFidoDeviceDiscovery, so we can simply
  // stick a short-lived instance into |discovery_factory_| and eliminate
  // |discovery_factory_testing_override_|.
  req_state_->discovery_factory_testing_override =
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
    RequestKey request_key,
    WebAuthenticationRequestProxy::RequestId request_id,
    blink::mojom::WebAuthnDOMExceptionDetailsPtr error,
    blink::mojom::MakeCredentialAuthenticatorResponsePtr response) {
  if (!CheckRequestKey(request_key)) {
    return;
  }
  DCHECK_EQ(*req_state_->pending_proxied_request_id, request_id);
  DCHECK(absl::holds_alternative<MakeCredentialCallback>(
             req_state_->response_callback) &&
         absl::get<MakeCredentialCallback>(req_state_->response_callback));
  req_state_->pending_proxied_request_id.reset();
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
    RequestKey request_key,
    WebAuthenticationRequestProxy::RequestId request_id,
    blink::mojom::WebAuthnDOMExceptionDetailsPtr error,
    blink::mojom::GetAssertionAuthenticatorResponsePtr response) {
  if (!CheckRequestKey(request_key)) {
    return;
  }
  DCHECK_EQ(*req_state_->pending_proxied_request_id, request_id);
  DCHECK(absl::holds_alternative<GetAssertionCallback>(
      req_state_->response_callback));
  req_state_->pending_proxied_request_id.reset();
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

AuthenticatorCommonImpl::RequestKey AuthenticatorCommonImpl::GetRequestKey() {
  return req_state_->request_key;
}

bool AuthenticatorCommonImpl::CheckRequestKey(RequestKey request_key) {
  return req_state_.get() && req_state_->request_key == request_key;
}

}  // namespace content
