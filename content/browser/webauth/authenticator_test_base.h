// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_TEST_BASE_H_
#define CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_TEST_BASE_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "content/browser/webauth/default_authenticator_request_client_delegate.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/web_authentication_delegate.h"
#include "content/public/browser/web_authentication_request_proxy.h"
#include "content/public/test/test_renderer_host.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"
#include "url/origin.h"

#if BUILDFLAG(IS_WIN)
#include "device/fido/win/fake_webauthn_api.h"
#include "device/fido/win/util.h"
#endif

namespace device {
class FidoDiscoveryFactory;
namespace test {
class VirtualFidoDeviceFactory;
}
}  // namespace device

namespace content {

using blink::mojom::AuthenticationExtensionsClientInputs;
using blink::mojom::AuthenticatorStatus;
using blink::mojom::GetCredentialOptions;
using blink::mojom::GetCredentialOptionsPtr;
using blink::mojom::PublicKeyCredentialCreationOptions;
using blink::mojom::PublicKeyCredentialCreationOptionsPtr;
using blink::mojom::PublicKeyCredentialRequestOptions;
using blink::mojom::PublicKeyCredentialRequestOptionsPtr;

typedef struct {
  std::string_view origin;
  // Either a relying party ID or a U2F AppID.
  std::string_view claimed_authority;
  AuthenticatorStatus expected_status;
} OriginClaimedAuthorityPair;

inline constexpr auto kValidRpTestCases =
    std::to_array<OriginClaimedAuthorityPair>({
        {"http://localhost", "localhost", AuthenticatorStatus::SUCCESS},
        {"https://myawesomedomain", "myawesomedomain",
         AuthenticatorStatus::SUCCESS},
        {"https://foo.bar.google.com", "foo.bar.google.com",
         AuthenticatorStatus::SUCCESS},
        {"https://foo.bar.google.com", "bar.google.com",
         AuthenticatorStatus::SUCCESS},
        {"https://foo.bar.google.com", "google.com",
         AuthenticatorStatus::SUCCESS},
        {"https://earth.login.awesomecompany", "login.awesomecompany",
         AuthenticatorStatus::SUCCESS},
        {"https://google.com:1337", "google.com", AuthenticatorStatus::SUCCESS},

        // Hosts with trailing dot valid for rpIds with or without trailing dot.
        // Hosts without trailing dots only matches rpIDs without trailing dot.
        // Two trailing dots only matches rpIDs with two trailing dots.
        {"https://google.com.", "google.com", AuthenticatorStatus::SUCCESS},
        {"https://google.com.", "google.com.", AuthenticatorStatus::SUCCESS},
        {"https://google.com..", "google.com..", AuthenticatorStatus::SUCCESS},

        // Leading dots are ignored in canonicalized hosts.
        {"https://.google.com", "google.com", AuthenticatorStatus::SUCCESS},
        {"https://..google.com", "google.com", AuthenticatorStatus::SUCCESS},
        {"https://.google.com", ".google.com", AuthenticatorStatus::SUCCESS},
        {"https://..google.com", ".google.com", AuthenticatorStatus::SUCCESS},
        {"https://accounts.google.com", ".google.com",
         AuthenticatorStatus::SUCCESS},
    });

inline constexpr auto kInvalidRpTestCases = std::to_array<
    OriginClaimedAuthorityPair>({
    {"https://google.com", "com", AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"http://google.com", "google.com", AuthenticatorStatus::INVALID_DOMAIN},
    {"http://myawesomedomain", "myawesomedomain",
     AuthenticatorStatus::INVALID_DOMAIN},
    {"https://google.com", "foo.bar.google.com",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"http://myawesomedomain", "randomdomain",
     AuthenticatorStatus::INVALID_DOMAIN},
    {"https://myawesomedomain", "randomdomain",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://notgoogle.com", "google.com)",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://not-google.com", "google.com)",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://evil.appspot.com", "appspot.com",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://evil.co.uk", "co.uk", AuthenticatorStatus::BAD_RELYING_PARTY_ID},

    {"https://google.com", "google.com.",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://google.com", "google.com..",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://google.com", ".google.com",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://google.com..", "google.com",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://.com", "com.", AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://.co.uk", "co.uk.", AuthenticatorStatus::BAD_RELYING_PARTY_ID},

    {"https://1.2.3", "1.2.3", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://1.2.3", "2.3", AuthenticatorStatus::INVALID_DOMAIN},

    {"https://127.0.0.1", "127.0.0.1", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://127.0.0.1", "27.0.0.1", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://127.0.0.1", ".0.0.1", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://127.0.0.1", "0.0.1", AuthenticatorStatus::INVALID_DOMAIN},

    {"https://[::127.0.0.1]", "127.0.0.1", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://[::127.0.0.1]", "[127.0.0.1]",
     AuthenticatorStatus::INVALID_DOMAIN},

    {"https://[::1]", "1", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://[::1]", "1]", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://[::1]", "::1", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://[::1]", "[::1]", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://[1::1]", "::1", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://[1::1]", "::1]", AuthenticatorStatus::INVALID_DOMAIN},
    {"https://[1::1]", "[::1]", AuthenticatorStatus::INVALID_DOMAIN},

    {"http://google.com:443", "google.com",
     AuthenticatorStatus::INVALID_DOMAIN},
    {"data:google.com", "google.com", AuthenticatorStatus::OPAQUE_DOMAIN},
    {"data:text/html,google.com", "google.com",
     AuthenticatorStatus::OPAQUE_DOMAIN},
    {"ws://google.com", "google.com", AuthenticatorStatus::INVALID_PROTOCOL},
    {"gopher://google.com", "google.com", AuthenticatorStatus::OPAQUE_DOMAIN},
    {"ftp://google.com", "google.com", AuthenticatorStatus::INVALID_PROTOCOL},
    {"file:///google.com", "google.com", AuthenticatorStatus::INVALID_PROTOCOL},
    // Use of webauthn from a WSS origin may be technically valid, but we
    // prohibit use on non-HTTPS origins. (At least for now.)
    {"wss://google.com", "google.com", AuthenticatorStatus::INVALID_PROTOCOL},

    {"data:,", "", AuthenticatorStatus::OPAQUE_DOMAIN},
    {"https://google.com", "", AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"ws:///google.com", "", AuthenticatorStatus::INVALID_PROTOCOL},
    {"wss:///google.com", "", AuthenticatorStatus::INVALID_PROTOCOL},
    {"gopher://google.com", "", AuthenticatorStatus::OPAQUE_DOMAIN},
    {"ftp://google.com", "", AuthenticatorStatus::INVALID_PROTOCOL},
    {"file:///google.com", "", AuthenticatorStatus::INVALID_PROTOCOL},

    // This case is acceptable according to spec, but both renderer
    // and browser handling currently do not permit it.
    {"https://login.awesomecompany", "awesomecompany",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},

    // These are AppID test cases, but should also be invalid relying party
    // examples too.
    {"https://example.com", "https://com/",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://example.com", "https://com/foo",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://example.com", "https://foo.com/",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://example.com", "http://example.com",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"http://example.com", "https://example.com",
     AuthenticatorStatus::INVALID_DOMAIN},
    {"https://127.0.0.1", "https://127.0.0.1",
     AuthenticatorStatus::INVALID_DOMAIN},
    {"https://www.notgoogle.com",
     "https://www.gstatic.com/securitykey/origins.json",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://www.google.com",
     "https://www.gstatic.com/securitykey/origins.json#x",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://www.google.com",
     "https://www.gstatic.com/securitykey/origins.json2",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://www.google.com", "https://gstatic.com/securitykey/origins.json",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://ggoogle.com", "https://www.gstatic.com/securitykey/origi",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
    {"https://com", "https://www.gstatic.com/securitykey/origins.json",
     AuthenticatorStatus::BAD_RELYING_PARTY_ID},
});

inline constexpr char kTestRelyingPartyId[] = "google.com";

// The size of credential IDs returned by GetTestCredentials().
inline constexpr size_t kTestCredentialIdLength = 32u;

device::PublicKeyCredentialUserEntity GetTestPublicKeyCredentialUserEntity();

device::AuthenticatorSelectionCriteria GetTestAuthenticatorSelectionCriteria();

std::vector<device::PublicKeyCredentialDescriptor> GetTestCredentials(
    size_t num_credentials = 1);

std::vector<device::PublicKeyCredentialParams::CredentialInfo>
GetTestPublicKeyCredentialParameters(int32_t algorithm_identifier);

device::PublicKeyCredentialRpEntity GetTestPublicKeyCredentialRPEntity();

PublicKeyCredentialCreationOptionsPtr
GetTestPublicKeyCredentialCreationOptions();

PublicKeyCredentialRequestOptionsPtr GetTestPublicKeyCredentialRequestOptions();

GetCredentialOptionsPtr GetTestGetCredentialOptions();

// TestAuthenticatorRequestDelegate is a test fake implementation of the
// AuthenticatorRequestClientDelegate embedder interface.
class TestAuthenticatorRequestDelegate
    : public DefaultAuthenticatorRequestClientDelegate {
 public:
  TestAuthenticatorRequestDelegate(
      RenderFrameHost* render_frame_host,
      base::OnceClosure action_callbacks_registered_callback,
      base::OnceClosure started_over_callback,
      bool simulate_user_cancelled,
      base::RepeatingCallback<void(bool)> enclave_discovered_callback,
      base::RepeatingCallback<void(const base::flat_set<device::FidoTransportProtocol>&)>
          transports_discovered_callback);

  TestAuthenticatorRequestDelegate(const TestAuthenticatorRequestDelegate&) =
      delete;
  TestAuthenticatorRequestDelegate& operator=(
      const TestAuthenticatorRequestDelegate&) = delete;

  ~TestAuthenticatorRequestDelegate() override;

  void RegisterActionCallbacks(
      base::OnceClosure cancel_callback,
      base::OnceClosure immediate_not_found_callback,
      base::RepeatingClosure start_over_callback,
      AccountPreselectedCallback account_preselected_callback,
      PasswordSelectedCallback password_selected_callback,
      device::FidoRequestHandlerBase::RequestCallback request_callback,
      base::OnceClosure cancel_ui_timeout_callback,
      base::RepeatingClosure bluetooth_adapter_power_on_callback,
      base::RepeatingCallback<
          void(device::FidoRequestHandlerBase::BlePermissionCallback)>
          ble_status_callback) override;

  void OnTransportAvailabilityEnumerated(
      device::FidoRequestHandlerBase::TransportAvailabilityInfo transport_info)
      override;

  bool DoesBlockRequestOnFailure(InterestingFailureReason reason) override;

  void ConfigureDiscoveries(
      const url::Origin& origin,
      const std::string& rp_id,
      RequestSource request_source,
      device::FidoRequestType request_type,
      std::optional<device::ResidentKeyRequirement> resident_key_requirement,
      device::UserVerificationRequirement user_verification_requirement,
      std::optional<std::string_view> user_name,
      base::span<const device::CableDiscoveryData> pairings_from_extension,
      bool is_enclave_authenticator_available,
      device::FidoDiscoveryFactory* fido_discovery_factory) override;

  base::OnceClosure action_callbacks_registered_callback_;
  base::OnceClosure cancel_callback_;
  base::OnceClosure started_over_callback_;
  base::OnceClosure start_over_callback_;
  bool does_block_request_on_failure_ = false;
  bool simulate_user_cancelled_ = false;
  base::RepeatingCallback<void(bool)> enclave_discovered_callback_;
  base::RepeatingCallback<void(const base::flat_set<device::FidoTransportProtocol>&)>
      transports_discovered_callback_;
};

// TestWebAuthenticationRequestProxy is a test fake implementation of the
// WebAuthenticationRequestProxy embedder interface.
class TestWebAuthenticationRequestProxy : public WebAuthenticationRequestProxy {
 public:
  struct Config {
    Config();
    ~Config();

    // If true, resolves all request event callbacks instantly.
    bool resolve_callbacks = true;

    // The return value of IsActive().
    bool is_active = true;

    // The fake response to SignalIsUVPAARequest().
    bool is_uvpaa = true;

    // Whether the request to SignalCreateRequest() should succeed.
    bool request_success = true;

    // If `request_success` is false, the name of the DOMError to be
    // returned.
    std::string request_error_name = "NotAllowedError";

    // If `request_success` is true, the fake response to be returned for an
    // onCreateRequest event.
    blink::mojom::MakeCredentialAuthenticatorResponsePtr
        make_credential_response = nullptr;

    // If `request_success` is true, the fake response to be returned for an
    // onGetRequest event.
    blink::mojom::GetAssertionAuthenticatorResponsePtr get_assertion_response =
        nullptr;
  };

  struct Observations {
    Observations();
    ~Observations();

    std::vector<PublicKeyCredentialCreationOptionsPtr> create_requests;
    std::vector<PublicKeyCredentialRequestOptionsPtr> get_requests;
    size_t num_isuvpaa = 0;
    size_t num_cancel = 0;
  };

  TestWebAuthenticationRequestProxy();
  ~TestWebAuthenticationRequestProxy() override;

  Config& config() { return config_; }

  Observations& observations() { return observations_; }

  bool IsActive(const url::Origin& caller_origin) override;

  RequestId SignalCreateRequest(
      const PublicKeyCredentialCreationOptionsPtr& options,
      CreateCallback callback) override;

  RequestId SignalGetRequest(
      const blink::mojom::PublicKeyCredentialRequestOptionsPtr& options,
      GetCallback callback) override;

  RequestId SignalIsUvpaaRequest(IsUvpaaCallback callback) override;

  void CancelRequest(RequestId request_id) override;

  void RunPendingCreateCallback();
  void RunPendingGetCallback();
  void RunPendingIsUvpaaCallback();
  bool HasPendingRequest();

 private:
  Config config_;
  Observations observations_;

  RequestId current_request_id_ = 0;
  CreateCallback pending_create_callback_;
  GetCallback pending_get_callback_;
  IsUvpaaCallback pending_is_uvpaa_callback_;
};

// TestWebAuthenticationDelegate is a test fake implementation of the
// WebAuthenticationDelegate embedder interface.
class TestWebAuthenticationDelegate : public WebAuthenticationDelegate {
 public:
  TestWebAuthenticationDelegate();
  ~TestWebAuthenticationDelegate() override;

  void IsUserVerifyingPlatformAuthenticatorAvailableOverride(
      RenderFrameHost*,
      base::OnceCallback<void(std::optional<bool>)> callback) override;

  bool OverrideCallerOriginAndRelyingPartyIdValidation(
      content::BrowserContext* browser_context,
      const url::Origin& origin,
      const std::string& rp_id) override;

  std::optional<std::string> MaybeGetRelyingPartyIdOverride(
      const std::string& claimed_rp_id,
      const url::Origin& caller_origin) override;

  bool ShouldPermitIndividualAttestation(
      content::BrowserContext* browser_context,
      const url::Origin& caller_origin,
      const std::string& relying_party_id) override;

  bool SupportsResidentKeys(RenderFrameHost*) override;

  bool IsFocused(WebContents* web_contents) override;

#if BUILDFLAG(IS_MAC)
  std::optional<TouchIdAuthenticatorConfig> GetTouchIdAuthenticatorConfig(
      BrowserContext* browser_context) override;
#endif

  WebAuthenticationRequestProxy* MaybeGetRequestProxy(
      content::BrowserContext* browser_context,
      const url::Origin& caller_origin) override;

  bool OriginMayUseRemoteDesktopClientOverride(
      content::BrowserContext* browser_context,
      const url::Origin& caller_origin) override;

  void BrowserProvidedPasskeysAvailable(
      BrowserContext* browser_context,
      base::OnceCallback<void(bool)> callback) override;

  // If set, the return value of IsUVPAA() will be overridden with this value.
  // Platform-specific implementations will not be invoked.
  std::optional<bool> is_uvpaa_override;

  // If set, the delegate will permit WebAuthn requests from chrome-extension
  // origins.
  bool permit_extensions = false;

  // Indicates whether individual attestation should be permitted by the
  // delegate.
  bool permit_individual_attestation = false;

  // A specific RP ID for which individual attestation will be permitted.
  std::optional<std::string> permit_individual_attestation_for_rp_id;

  // Indicates whether resident key operations should be permitted by the
  // delegate.
  bool supports_resident_keys = false;

  // The return value of the focus check issued at the end of a request.
  bool is_focused = true;

#if BUILDFLAG(IS_MAC)
  // Configuration data for the macOS platform authenticator.
  std::optional<TouchIdAuthenticatorConfig> touch_id_authenticator_config;
#endif

  // The WebAuthenticationRequestProxy returned by |MaybeGetRequestProxy|.
  std::unique_ptr<TestWebAuthenticationRequestProxy> request_proxy = nullptr;

  // The origin permitted to use the RemoteDesktopClientOverride extension.
  std::optional<url::Origin> remote_desktop_client_override_origin;

  // Return value of `BrowserProvidedPasskeysAvailable()`.
  bool browser_provided_passkeys_available = false;
};

// TestAuthenticatorContentBrowserClient is a test fake implementation of the
// ContentBrowserClient interface that injects |TestWebAuthenticationDelegate|
// and |TestAuthenticatorRequestDelegate| instances into |AuthenticatorImpl|.
class TestAuthenticatorContentBrowserClient : public ContentBrowserClient {
 public:
  TestAuthenticatorContentBrowserClient();
  ~TestAuthenticatorContentBrowserClient() override;

  TestWebAuthenticationDelegate* GetTestWebAuthenticationDelegate();

  // ContentBrowserClient:
  WebAuthenticationDelegate* GetWebAuthenticationDelegate() override;

  bool IsSecurityLevelAcceptableForWebAuthn(content::RenderFrameHost* rfh,
                                            const url::Origin& origin) override;
  bool ShouldDisallowCredentialRequest(WebContents* web_contents) override;

  std::unique_ptr<AuthenticatorRequestClientDelegate>
  GetWebAuthenticationRequestDelegate(
      RenderFrameHost* render_frame_host) override;

  TestWebAuthenticationDelegate web_authentication_delegate;

  // If set, this closure will be called when the subsequently constructed
  // delegate is informed that the request has started.
  base::OnceClosure action_callbacks_registered_callback;

  // This emulates scenarios where a nullptr RequestClientDelegate is returned
  // because a request is already in progress.
  bool return_null_delegate = false;

  // If started_over_callback_ is set to a non-null callback, the request will
  // be restarted after action callbacks are registered, and
  // |started_over_callback| will replace
  // |action_callbacks_registered_callback|. This should then be called the
  // second time action callbacks are registered. It also causes
  // DoesBlockRequestOnFailure to return true, once.
  base::OnceClosure started_over_callback_;

  // This simulates the user immediately cancelling the request after transport
  // availability info is enumerated.
  bool simulate_user_cancelled_ = false;

  // The return value of IsSecurityLevelAcceptableForWebAuthn.
  bool is_webauthn_security_level_acceptable = true;

  // The return value of ShouldDisallowCredentialRequest.
  bool should_disallow_credential_request = false;

  // Whether discovery of the enclave authenticator was requested or not.
  std::optional<bool> enclave_authenticator_should_be_discovered_;

  // The set of transports allowed for a request.
  base::flat_set<device::FidoTransportProtocol> discovered_transports_;

 private:
  base::WeakPtrFactory<TestAuthenticatorContentBrowserClient> weak_factory_{this};
};

class AuthenticatorTestBase : public RenderViewHostTestHarness {
 protected:
  AuthenticatorTestBase();
  ~AuthenticatorTestBase() override;

  static void SetUpTestSuite();

  void SetUp() override;
  void TearDown() override;

  virtual void ResetVirtualDevice();
  virtual void ReplaceDiscoveryFactory(
      std::unique_ptr<device::FidoDiscoveryFactory> device_factory);
  void SetMojoErrorHandler(
      base::RepeatingCallback<void(const std::string&)> callback);

  raw_ptr<device::test::VirtualFidoDeviceFactory> virtual_device_factory_ =
      nullptr;
#if BUILDFLAG(IS_WIN)
  device::FakeWinWebAuthnApi fake_win_webauthn_api_;
  device::WinWebAuthnApi::ScopedOverride win_webauthn_api_override_{
      &fake_win_webauthn_api_};
  std::unique_ptr<device::fido::win::ScopedBiometricsOverride>
      biometrics_override_;
#endif

 private:
  void OnMojoError(const std::string& error);

  base::RepeatingCallback<void(const std::string&)> mojo_error_handler_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBAUTH_AUTHENTICATOR_TEST_BASE_H_
