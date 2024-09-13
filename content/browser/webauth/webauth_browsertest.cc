// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/command_line.h"
#include "base/containers/flat_set.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/payments/stub_payment_credential.h"
#include "content/browser/renderer_host/back_forward_cache_disable.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webauth/authenticator_environment.h"
#include "content/browser/webauth/authenticator_impl.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/scoped_authenticator_environment_for_testing.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_content_browser_client.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/content_mock_cert_verifier.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/did_commit_navigation_interceptor.h"
#include "content/test/fake_network_url_loader_factory.h"
#include "device/fido/fake_fido_discovery.h"
#include "device/fido/features.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/fido_transport_protocol.h"
#include "device/fido/fido_types.h"
#include "device/fido/public_key_credential_params.h"
#include "device/fido/virtual_fido_device.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include "device/fido/win/fake_webauthn_api.h"
#endif

namespace content {

namespace {

using blink::mojom::Authenticator;
using blink::mojom::AuthenticatorStatus;
using blink::mojom::GetAssertionAuthenticatorResponsePtr;
using blink::mojom::MakeCredentialAuthenticatorResponsePtr;
using blink::mojom::WebAuthnDOMExceptionDetailsPtr;

using TestCreateFuture =
    base::test::TestFuture<AuthenticatorStatus,
                           MakeCredentialAuthenticatorResponsePtr,
                           WebAuthnDOMExceptionDetailsPtr>;

using TestGetFuture =
    base::test::TestFuture<AuthenticatorStatus,
                           GetAssertionAuthenticatorResponsePtr,
                           WebAuthnDOMExceptionDetailsPtr>;

constexpr char kOkMessage[] = "OK";

constexpr char kPublicKeyErrorMessage[] =
    "TypeError: Failed to execute 'create' on 'CredentialsContainer': "
    "Failed to read the 'publicKey' property from 'CredentialCreationOptions': "
    "Failed to read the 'rp' property from 'PublicKeyCredentialCreationOptions'"
    ": The provided value is not of type 'PublicKeyCredentialRpEntity'.";

constexpr char kNotAllowedErrorMessage[] =
    "NotAllowedError: The operation either timed out or was not "
    "allowed. See: "
    "https://www.w3.org/TR/webauthn-2/#sctn-privacy-considerations-client.";

#if BUILDFLAG(IS_WIN)
constexpr char kInvalidStateErrorMessage[] =
    "InvalidStateError: The user attempted to register an "
    "authenticator that contains one of the credentials already registered "
    "with the relying party.";
#endif  // BUILDFLAG(IS_WIN)

constexpr char kResidentCredentialsErrorMessage[] =
    "NotSupportedError: Resident credentials or empty "
    "'allowCredentials' lists are not supported at this time.";

constexpr char kRelyingPartySecurityErrorMessage[] =
    "SecurityError: The relying party ID is not a registrable domain "
    "suffix of, nor equal to the current domain.";

constexpr char kAbortErrorMessage[] =
    "AbortError: signal is aborted without reason";

constexpr char kAbortReasonMessage[] = "Error";

constexpr char kCreatePermissionsPolicyMissingMessage[] =
    "NotAllowedError: The 'publickey-credentials-create' feature is "
    "not enabled in this document. Permissions Policy may be used to delegate "
    "Web Authentication capabilities to cross-origin child frames.";

constexpr char kCreateWithPaymentPermissionsPolicyMissingMessage[] =
    "NotSupportedError: The 'payment' or 'publickey-credentials-create' "
    "features are not enabled in this document. Permissions Policy may be used "
    "to delegate Web Payment capabilities to cross-origin child frames.";

constexpr char kGetPermissionsPolicyMissingMessage[] =
    "NotAllowedError: The 'publickey-credentials-get' feature is "
    "not enabled in this document. Permissions Policy may be used to delegate "
    "Web Authentication capabilities to cross-origin child frames.";

constexpr char kAllowCredentialsRangeErrorMessage[] =
    "RangeError: The `allowCredentials` attribute exceeds the maximum "
    "allowed size (64).";

constexpr char kExcludeCredentialsRangeErrorMessage[] =
    "RangeError: The `excludeCredentials` attribute exceeds the "
    "maximum allowed size (64).";

constexpr char kRpIdContentTypeMessage[] =
    "SecurityError: The relying party ID is not a registrable domain suffix "
    "of, nor equal to the current domain. Subsequently, the "
    ".well-known/webauthn resource of the claimed RP ID had the wrong "
    "content-type. (It should be application/json.)";

constexpr char kRpIdNoEntryMessage[] =
    "SecurityError: The relying party ID is not a registrable domain suffix "
    "of, nor equal to the current domain. Subsequently, fetching the "
    ".well-known/webauthn resource of the claimed RP ID was "
    "successful, but no listed origin matched the caller.";

// Templates to be used with base::ReplaceStringPlaceholders. Can be
// modified to include up to 9 replacements. The default values for
// any additional replacements added should also be added to the
// CreateParameters struct.
constexpr char kCreatePublicKeyTemplate[] =
    "navigator.credentials.create({ publicKey: {"
    "  challenge: new TextEncoder().encode('climb a mountain'),"
    "  rp: { id: '$3', name: 'Acme'},"
    "  user: { "
    "    id: new TextEncoder().encode('1098237235409872'),"
    "    name: 'avery.a.jones@example.com',"
    "    displayName: 'Avery A. Jones'},"
    "  pubKeyCredParams: [{ type: 'public-key', alg: '$4'}],"
    "  timeout: _timeout_,"
    "  excludeCredentials: $7,"
    "  authenticatorSelection: {"
    "     requireResidentKey: $1,"
    "     userVerification: '$2',"
    "     authenticatorAttachment: '$5',"
    "  },"
    "  attestation: '$6',"
    "  extensions: {payment: {isPayment: $8}},"
    "}}).then(c => 'OK',"
    "         e => e.toString())";

constexpr char kCreatePublicKeyWithAbortSignalTemplate[] =
    "navigator.credentials.create({ publicKey: {"
    "  challenge: new TextEncoder().encode('climb a mountain'),"
    "  rp: { id: '$3', name: 'Acme'},"
    "  user: { "
    "    id: new TextEncoder().encode('1098237235409872'),"
    "    name: 'avery.a.jones@example.com',"
    "    displayName: 'Avery A. Jones'},"
    "  pubKeyCredParams: [{ type: 'public-key', alg: '$4'}],"
    "  timeout: _timeout_,"
    "  excludeCredentials: $7,"
    "  authenticatorSelection: {"
    "     requireResidentKey: $1,"
    "     userVerification: '$2',"
    "     authenticatorAttachment: '$5',"
    "  },"
    "  attestation: '$6',"
    "  extensions: {payment: {isPayment: $8}},"
    "}, signal: _signal_}"
    ").then(c => 'OK',"
    "       e => e.toString())";

constexpr char kShortTimeout[] = "100";

// Default values for kCreatePublicKeyTemplate.
struct CreateParameters {
  std::string rp_id = "acme.com";
  bool require_resident_key = false;
  std::string user_verification = "preferred";
  std::string authenticator_attachment = "cross-platform";
  std::string algorithm_identifier = "-7";
  std::string attestation = "none";
  std::string exclude_credentials = "[]";
  std::string signal = "";
  std::string timeout = "10000";
  bool is_payment = false;
};

std::string BuildCreateCallWithParameters(const CreateParameters& parameters) {
  std::vector<std::string> substitutions;
  substitutions.push_back(parameters.require_resident_key ? "true" : "false");
  substitutions.push_back(parameters.user_verification);
  substitutions.push_back(parameters.rp_id);
  substitutions.push_back(parameters.algorithm_identifier);
  substitutions.push_back(parameters.authenticator_attachment);
  substitutions.push_back(parameters.attestation);
  substitutions.push_back(parameters.exclude_credentials);
  substitutions.push_back(parameters.is_payment ? "true" : "false");

  std::string result;
  if (parameters.signal.empty()) {
    result = base::ReplaceStringPlaceholders(kCreatePublicKeyTemplate,
                                             substitutions, nullptr);
  } else {
    result = base::ReplaceStringPlaceholders(
        kCreatePublicKeyWithAbortSignalTemplate, substitutions, nullptr);
    base::ReplaceFirstSubstringAfterOffset(&result, 0, "_signal_",
                                           parameters.signal);
  }

  base::ReplaceFirstSubstringAfterOffset(&result, 0, "_timeout_",
                                         parameters.timeout);
  return result;
}

constexpr char kGetPublicKeyTemplate[] =
    "navigator.credentials.get({ publicKey: {"
    "  challenge: new TextEncoder().encode('climb a mountain'),"
    "  userVerification: '$1',"
    "  allowCredentials: $2,"
    "  timeout: $3,"
    "  rpId: '$4'}"
    "}).then(c => 'OK',"
    "        e => e.toString())";

constexpr char kGetPublicKeyWithAbortSignalTemplate[] =
    "navigator.credentials.get({ publicKey: {"
    "  challenge: new TextEncoder().encode('climb a mountain'),"
    "  userVerification: '$1',"
    "  allowCredentials: $2,"
    "  timeout: $3,"
    "  rpId: '$4',"
    "}, signal: $5}"
    ").catch(c => c.toString())";

// Default values for kGetPublicKeyTemplate.
struct GetParameters {
  std::string user_verification = "preferred";
  std::string allow_credentials =
      "[{type: 'public-key',"
      "  id: new TextEncoder().encode('allowedCredential'),"
      "  transports: ['usb', 'nfc', 'ble']}]";
  std::string signal = "";
  std::string timeout = "10000";
  std::string rp_id = "acme.com";
};

std::string BuildGetCallWithParameters(const GetParameters& parameters) {
  std::vector<std::string> substitutions;
  substitutions.push_back(parameters.user_verification);
  substitutions.push_back(parameters.allow_credentials);
  substitutions.push_back(parameters.timeout);
  substitutions.push_back(parameters.rp_id);
  if (parameters.signal.empty()) {
    return base::ReplaceStringPlaceholders(kGetPublicKeyTemplate, substitutions,
                                           nullptr);
  }
  substitutions.push_back(parameters.signal);
  return base::ReplaceStringPlaceholders(kGetPublicKeyWithAbortSignalTemplate,
                                         substitutions, nullptr);
}

// Helper class that executes the given |closure| the very last moment before
// the next navigation commits in a given WebContents.
class ClosureExecutorBeforeNavigationCommit
    : public DidCommitNavigationInterceptor {
 public:
  ClosureExecutorBeforeNavigationCommit(WebContents* web_contents,
                                        base::OnceClosure closure)
      : DidCommitNavigationInterceptor(web_contents),
        closure_(std::move(closure)) {}

  ClosureExecutorBeforeNavigationCommit(
      const ClosureExecutorBeforeNavigationCommit&) = delete;
  ClosureExecutorBeforeNavigationCommit& operator=(
      const ClosureExecutorBeforeNavigationCommit&) = delete;

  ~ClosureExecutorBeforeNavigationCommit() override = default;

 protected:
  bool WillProcessDidCommitNavigation(
      RenderFrameHost* render_frame_host,
      NavigationRequest* navigation_request,
      mojom::DidCommitProvisionalLoadParamsPtr* params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params)
      override {
    if (closure_) {
      std::move(closure_).Run();
    }
    return true;
  }

 private:
  base::OnceClosure closure_;
};

// Cancels all navigations in a WebContents while in scope.
class ScopedNavigationCancellingThrottleInstaller : public WebContentsObserver {
 public:
  explicit ScopedNavigationCancellingThrottleInstaller(
      WebContents* web_contents)
      : WebContentsObserver(web_contents) {}

  ScopedNavigationCancellingThrottleInstaller(
      const ScopedNavigationCancellingThrottleInstaller&) = delete;
  ScopedNavigationCancellingThrottleInstaller& operator=(
      const ScopedNavigationCancellingThrottleInstaller&) = delete;

  ~ScopedNavigationCancellingThrottleInstaller() override = default;

 protected:
  class CancellingThrottle : public NavigationThrottle {
   public:
    explicit CancellingThrottle(NavigationHandle* handle)
        : NavigationThrottle(handle) {}

    CancellingThrottle(const CancellingThrottle&) = delete;
    CancellingThrottle& operator=(const CancellingThrottle&) = delete;

    ~CancellingThrottle() override = default;

   protected:
    const char* GetNameForLogging() override {
      return "ScopedNavigationCancellingThrottleInstaller::CancellingThrottle";
    }

    ThrottleCheckResult WillStartRequest() override {
      return ThrottleCheckResult(CANCEL);
    }
  };

  void DidStartNavigation(NavigationHandle* navigation_handle) override {
    navigation_handle->RegisterThrottleForTesting(
        std::make_unique<CancellingThrottle>(navigation_handle));
  }
};

struct WebAuthBrowserTestState {
  // Set when |IsFocused| is called.
  bool focus_checked = false;

  // This is incremented when an |AuthenticatorRequestClientDelegate| is
  // created.
  int delegate_create_count = 0;
};

class WebAuthBrowserTestWebAuthenticationDelegate
    : public WebAuthenticationDelegate {
 public:
  explicit WebAuthBrowserTestWebAuthenticationDelegate(
      WebAuthBrowserTestState* test_state)
      : test_state_(test_state) {}

  bool IsFocused(content::WebContents* web_contents) override {
    test_state_->focus_checked = true;
    return WebAuthenticationDelegate::IsFocused(web_contents);
  }

 private:
  const raw_ptr<WebAuthBrowserTestState> test_state_;
};

class WebAuthBrowserTestClientDelegate
    : public AuthenticatorRequestClientDelegate {
 public:
  explicit WebAuthBrowserTestClientDelegate(WebAuthBrowserTestState* test_state)
      : test_state_(test_state) {}

  WebAuthBrowserTestClientDelegate(const WebAuthBrowserTestClientDelegate&) =
      delete;
  WebAuthBrowserTestClientDelegate& operator=(
      const WebAuthBrowserTestClientDelegate&) = delete;

 private:
  const raw_ptr<WebAuthBrowserTestState> test_state_;
};

// Implements ContentBrowserClient and allows webauthn-related calls to be
// mocked.
class WebAuthBrowserTestContentBrowserClient
    : public ContentBrowserTestContentBrowserClient {
 public:
  explicit WebAuthBrowserTestContentBrowserClient(
      WebAuthBrowserTestState* test_state)
      : test_state_(test_state) {}

  WebAuthBrowserTestContentBrowserClient(
      const WebAuthBrowserTestContentBrowserClient&) = delete;
  WebAuthBrowserTestContentBrowserClient& operator=(
      const WebAuthBrowserTestContentBrowserClient&) = delete;

  WebAuthenticationDelegate* GetWebAuthenticationDelegate() override {
    return &web_authentication_delegate_;
  }

  std::unique_ptr<AuthenticatorRequestClientDelegate>
  GetWebAuthenticationRequestDelegate(
      RenderFrameHost* render_frame_host) override {
    test_state_->delegate_create_count++;
    return std::make_unique<WebAuthBrowserTestClientDelegate>(test_state_);
  }

  void CreatePaymentCredential(
      RenderFrameHost* render_frame_host,
      mojo::PendingReceiver<payments::mojom::PaymentCredential> receiver)
      override {
    StubPaymentCredential::Create(render_frame_host, std::move(receiver));
  }

  scoped_refptr<network::SharedURLLoaderFactory>
  GetSystemSharedURLLoaderFactory() override {
    // This is used by `WebAuthRequestSecurityChecker` to do cross-domain RP ID
    // validations.
    return fake_url_loader_factory_;
  }

  // set_webauthn_origins_response sets the fake HTTP response that will be
  // returned for all requests for `.well-known/webauthn` requests.
  void set_webauthn_origins_response(std::string_view content_type,
                                     std::string_view authorized_origin) {
    auto fake_url_loader_factory =
        std::make_unique<FakeNetworkURLLoaderFactory>(
            base::StrCat(
                {"HTTP/1.1 200 OK\nContent-Type: ", content_type, "\n\n"}),
            base::StrCat({"{\"origins\": [\"", authorized_origin, "\"]}"}),
            /* network_accessed */ true, net::OK);
    fake_url_loader_factory_ = base::MakeRefCounted<FakeSharedURLLoaderFactory>(
        std::move(fake_url_loader_factory));
  }

  // sinkhole_webauthn_origins_requests causes the RP ID validation request to
  // be dropped.
  void sinkhole_webauthn_origins_requests() {
    fake_url_loader_factory_ =
        base::MakeRefCounted<NoopSharedURLLoaderFactory>();
  }

 private:
  class FakeSharedURLLoaderFactory : public network::SharedURLLoaderFactory {
   public:
    explicit FakeSharedURLLoaderFactory(
        std::unique_ptr<FakeNetworkURLLoaderFactory> fake)
        : fake_(std::move(fake)) {}

    void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
        override {
      CHECK(false);
    }

    std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override {
      CHECK(false);
      return nullptr;
    }

    void CreateLoaderAndStart(
        mojo::PendingReceiver<network::mojom::URLLoader> receiver,
        int32_t request_id,
        uint32_t options,
        const network::ResourceRequest& url_request,
        mojo::PendingRemote<network::mojom::URLLoaderClient> client,
        const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
        override {
      fake_->CreateLoaderAndStart(std::move(receiver), request_id, options,
                                  url_request, std::move(client),
                                  traffic_annotation);
    }

   private:
    friend class base::RefCounted<FakeSharedURLLoaderFactory>;
    ~FakeSharedURLLoaderFactory() override = default;
    std::unique_ptr<FakeNetworkURLLoaderFactory> fake_;
  };

  // NoopSharedURLLoaderFactory ignores requests and thus makes it look like
  // fetches take forever.
  class NoopSharedURLLoaderFactory : public network::SharedURLLoaderFactory {
   public:
    void Clone(mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver)
        override {
      CHECK(false);
    }

    std::unique_ptr<network::PendingSharedURLLoaderFactory> Clone() override {
      CHECK(false);
      return nullptr;
    }

    void CreateLoaderAndStart(
        mojo::PendingReceiver<network::mojom::URLLoader> receiver,
        int32_t request_id,
        uint32_t options,
        const network::ResourceRequest& url_request,
        mojo::PendingRemote<network::mojom::URLLoaderClient> client,
        const net::MutableNetworkTrafficAnnotationTag& traffic_annotation)
        override {
      receiver_ = std::move(receiver);
      client_ = std::move(client);
    }

   private:
    friend class base::RefCounted<NoopSharedURLLoaderFactory>;
    ~NoopSharedURLLoaderFactory() override = default;

    mojo::PendingReceiver<network::mojom::URLLoader> receiver_;
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_;
  };

  const raw_ptr<WebAuthBrowserTestState> test_state_;
  const std::string source_origin_;
  scoped_refptr<network::SharedURLLoaderFactory> fake_url_loader_factory_;
  WebAuthBrowserTestWebAuthenticationDelegate web_authentication_delegate_{
      test_state_};
};

// Test fixture base class for common tasks.
class WebAuthBrowserTestBase : public content::ContentBrowserTest {
 public:
  WebAuthBrowserTestBase(const WebAuthBrowserTestBase&) = delete;
  WebAuthBrowserTestBase& operator=(const WebAuthBrowserTestBase&) = delete;

 protected:
  WebAuthBrowserTestBase() = default;

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();
    mock_cert_verifier_.mock_cert_verifier()->set_default_result(net::OK);

    host_resolver()->AddRule("*", "127.0.0.1");
    https_server().ServeFilesFromSourceDirectory(GetTestDataFilePath());
    ASSERT_TRUE(https_server().Start());

    test_client_ =
        std::make_unique<WebAuthBrowserTestContentBrowserClient>(&test_state_);

    EXPECT_TRUE(
        NavigateToURL(shell(), GetHttpsURL("www.acme.com", "/title1.html")));
  }

  void TearDown() override {
    test_client_.reset();
    ContentBrowserTest::TearDown();
  }

  GURL GetHttpsURL(const std::string& hostname,
                   const std::string& relative_url) {
    return https_server_.GetURL(hostname, relative_url);
  }

  device::test::VirtualFidoDeviceFactory* InjectVirtualFidoDeviceFactory() {
    auto owned_virtual_device_factory =
        std::make_unique<device::test::VirtualFidoDeviceFactory>();
    auto* virtual_device_factory = owned_virtual_device_factory.get();
    auth_env_.reset();
    auth_env_ = std::make_unique<ScopedAuthenticatorEnvironmentForTesting>(
        std::move(owned_virtual_device_factory));
    return virtual_device_factory;
  }

  net::EmbeddedTestServer& https_server() { return https_server_; }

  WebAuthBrowserTestState* test_state() { return &test_state_; }

  WebAuthBrowserTestContentBrowserClient* test_client() {
    return test_client_.get();
  }

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    mock_cert_verifier_.SetUpCommandLine(command_line);
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

  void SetUpInProcessBrowserTestFixture() override {
    ContentBrowserTest::SetUpInProcessBrowserTestFixture();
    mock_cert_verifier_.SetUpInProcessBrowserTestFixture();
  }

  void TearDownInProcessBrowserTestFixture() override {
    ContentBrowserTest::TearDownInProcessBrowserTestFixture();
    mock_cert_verifier_.TearDownInProcessBrowserTestFixture();
  }

 private:
  content::ContentMockCertVerifier mock_cert_verifier_;

  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  std::unique_ptr<WebAuthBrowserTestContentBrowserClient> test_client_;
  std::unique_ptr<ScopedAuthenticatorEnvironmentForTesting> auth_env_;
  WebAuthBrowserTestState test_state_;
};

// WebAuthLocalClientBrowserTest ----------------------------------------------

// Browser test fixture where the blink::mojom::Authenticator interface is
// accessed from a testing client in the browser process.
class WebAuthLocalClientBrowserTest : public WebAuthBrowserTestBase {
 public:
  WebAuthLocalClientBrowserTest() = default;

  WebAuthLocalClientBrowserTest(const WebAuthLocalClientBrowserTest&) = delete;
  WebAuthLocalClientBrowserTest& operator=(
      const WebAuthLocalClientBrowserTest&) = delete;

  ~WebAuthLocalClientBrowserTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    WebAuthBrowserTestBase::SetUpOnMainThread();
    ConnectToAuthenticator();
  }

  void TearDownOnMainThread() override {
    authenticator_remote_.reset();
    WebAuthBrowserTestBase::TearDownOnMainThread();
  }

  void ConnectToAuthenticator() {
    if (authenticator_remote_.is_bound()) {
      authenticator_remote_.reset();
    }
    static_cast<RenderFrameHostImpl*>(
        shell()->web_contents()->GetPrimaryMainFrame())
        ->GetWebAuthenticationService(
            authenticator_remote_.BindNewPipeAndPassReceiver());
  }

  blink::mojom::PublicKeyCredentialCreationOptionsPtr
  BuildBasicCreateOptions() {
    device::PublicKeyCredentialRpEntity rp("acme.com");
    rp.name = "acme.com";

    std::vector<uint8_t> kTestUserId{0, 0, 0};
    device::PublicKeyCredentialUserEntity user(kTestUserId);
    user.name = "name";
    user.display_name = "displayName";

    static constexpr int32_t kCOSEAlgorithmIdentifierES256 = -7;
    device::PublicKeyCredentialParams::CredentialInfo param;
    param.type = device::CredentialType::kPublicKey;
    param.algorithm = kCOSEAlgorithmIdentifierES256;
    std::vector<device::PublicKeyCredentialParams::CredentialInfo> parameters;
    parameters.push_back(param);

    std::vector<uint8_t> kTestChallenge{0, 0, 0};
    auto mojo_options = blink::mojom::PublicKeyCredentialCreationOptions::New();
    mojo_options->relying_party = rp;
    mojo_options->user = user;
    mojo_options->challenge = kTestChallenge;
    mojo_options->public_key_parameters = parameters;
    mojo_options->timeout = base::Seconds(30);
    return mojo_options;
  }

  blink::mojom::PublicKeyCredentialRequestOptionsPtr BuildBasicGetOptions() {
    std::vector<device::PublicKeyCredentialDescriptor> credentials;
    base::flat_set<device::FidoTransportProtocol> transports;
    transports.emplace(device::FidoTransportProtocol::kUsbHumanInterfaceDevice);

    device::PublicKeyCredentialDescriptor descriptor(
        device::CredentialType::kPublicKey,
        device::fido_parsing_utils::Materialize(
            device::test_data::kTestGetAssertionCredentialId),
        transports);
    credentials.push_back(descriptor);

    std::vector<uint8_t> kTestChallenge{0, 0, 0};
    auto mojo_options = blink::mojom::PublicKeyCredentialRequestOptions::New();
    mojo_options->extensions =
        blink::mojom::AuthenticationExtensionsClientInputs::New();
    mojo_options->challenge = kTestChallenge;
    mojo_options->timeout = base::Seconds(30);
    mojo_options->relying_party_id = "acme.com";
    mojo_options->allow_credentials = std::move(credentials);
    mojo_options->user_verification =
        device::UserVerificationRequirement::kPreferred;
    return mojo_options;
  }

  void WaitForConnectionError() {
    ASSERT_TRUE(authenticator_remote_);
    ASSERT_TRUE(authenticator_remote_.is_bound());
    if (!authenticator_remote_.is_connected()) {
      return;
    }

    base::RunLoop run_loop;
    authenticator_remote_.set_disconnect_handler(run_loop.QuitClosure());
    run_loop.Run();
  }

  blink::mojom::Authenticator* authenticator() {
    return authenticator_remote_.get();
  }

  raw_ptr<device::test::FakeFidoDiscoveryFactory> discovery_factory_;

 private:
  mojo::Remote<blink::mojom::Authenticator> authenticator_remote_;
};

// Tests that no crash occurs when the implementation is destroyed with a
// pending navigator.credentials.create({publicKey: ...}) call.
IN_PROC_BROWSER_TEST_F(WebAuthLocalClientBrowserTest,
                       CreatePublicKeyCredentialThenNavigateAway) {
  device::test::VirtualFidoDeviceFactory* virtual_device_factory =
      InjectVirtualFidoDeviceFactory();
  virtual_device_factory->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting(
          [&](device::VirtualFidoDevice* device) { return false; });

  TestCreateFuture create_future;
  authenticator()->MakeCredential(BuildBasicCreateOptions(),
                                  create_future.GetCallback());
  ASSERT_FALSE(create_future.IsReady());
  EXPECT_TRUE(
      NavigateToURL(shell(), GetHttpsURL("www.acme.com", "/title2.html")));
  WaitForConnectionError();

  // The next active document should be able to successfully call
  // navigator.credentials.create({publicKey: ...}) again.
  ConnectToAuthenticator();
  InjectVirtualFidoDeviceFactory();
  TestCreateFuture create_future2;
  authenticator()->MakeCredential(BuildBasicCreateOptions(),
                                  create_future2.GetCallback());
  EXPECT_TRUE(create_future2.Wait());
  EXPECT_EQ(std::get<0>(create_future2.Get()), AuthenticatorStatus::SUCCESS);
}

// Tests that no crash occurs when the implementation is destroyed with a
// pending navigator.credentials.get({publicKey: ...}) call.
IN_PROC_BROWSER_TEST_F(WebAuthLocalClientBrowserTest,
                       GetPublicKeyCredentialThenNavigateAway) {
  device::test::VirtualFidoDeviceFactory* virtual_device_factory =
      InjectVirtualFidoDeviceFactory();
  virtual_device_factory->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting(
          [&](device::VirtualFidoDevice* device) { return false; });

  TestGetFuture get_future;
  authenticator()->GetAssertion(BuildBasicGetOptions(),
                                get_future.GetCallback());
  ASSERT_FALSE(get_future.IsReady());
  EXPECT_TRUE(
      NavigateToURL(shell(), GetHttpsURL("www.acme.com", "/title2.html")));
  WaitForConnectionError();

  // The next active document should be able to successfully call
  // navigator.credentials.get({publicKey: ...}) again.
  ConnectToAuthenticator();
  InjectVirtualFidoDeviceFactory();
  TestGetFuture get_future2;
  authenticator()->GetAssertion(BuildBasicGetOptions(),
                                get_future2.GetCallback());
  EXPECT_TRUE(get_future2.Wait());
  EXPECT_EQ(std::get<0>(get_future2.Get()),
            AuthenticatorStatus::NOT_ALLOWED_ERROR);
}

// Tests that the blink::mojom::Authenticator connection is not closed on a
// cancelled navigation.
IN_PROC_BROWSER_TEST_F(WebAuthLocalClientBrowserTest,
                       CreatePublicKeyCredentialAfterCancelledNavigation) {
  ScopedNavigationCancellingThrottleInstaller navigation_canceller(
      shell()->web_contents());

  // This navigation should be canceled and hence should not succeed.
  EXPECT_FALSE(
      NavigateToURL(shell(), GetHttpsURL("www.acme.com", "/title2.html")));

  InjectVirtualFidoDeviceFactory();
  TestCreateFuture create_future;
  authenticator()->MakeCredential(BuildBasicCreateOptions(),
                                  create_future.GetCallback());
  EXPECT_TRUE(create_future.Wait());
  EXPECT_EQ(std::get<0>(create_future.Get()), AuthenticatorStatus::SUCCESS);
}

// Tests that a navigator.credentials.create({publicKey: ...}) issued at the
// moment just before a navigation commits is not serviced.
IN_PROC_BROWSER_TEST_F(WebAuthLocalClientBrowserTest,
                       CreatePublicKeyCredentialRacingWithNavigation) {
  InjectVirtualFidoDeviceFactory();

  TestCreateFuture create_future;
  auto request_options = BuildBasicCreateOptions();

  ClosureExecutorBeforeNavigationCommit executor(
      shell()->web_contents(), base::BindLambdaForTesting([&]() {
        authenticator()->MakeCredential(std::move(request_options),
                                        create_future.GetCallback());
      }));

  EXPECT_TRUE(
      NavigateToURL(shell(), GetHttpsURL("www.acme.com", "/title2.html")));
  WaitForConnectionError();

  // The next active document should be able to successfully call
  // navigator.credentials.create({publicKey: ...}) again.
  ConnectToAuthenticator();
  TestCreateFuture create_future2;
  authenticator()->MakeCredential(BuildBasicCreateOptions(),
                                  create_future2.GetCallback());
  EXPECT_TRUE(create_future2.Wait());
  EXPECT_EQ(AuthenticatorStatus::SUCCESS, std::get<0>(create_future2.Get()));
}

// Regression test for https://crbug.com/818219.
IN_PROC_BROWSER_TEST_F(WebAuthLocalClientBrowserTest,
                       CreatePublicKeyCredentialTwiceInARow) {
  device::test::VirtualFidoDeviceFactory* virtual_device_factory =
      InjectVirtualFidoDeviceFactory();
  virtual_device_factory->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting(
          [&](device::VirtualFidoDevice* device) { return false; });

  TestCreateFuture future_1;
  TestCreateFuture future_2;
  authenticator()->MakeCredential(BuildBasicCreateOptions(),
                                  future_1.GetCallback());
  authenticator()->MakeCredential(BuildBasicCreateOptions(),
                                  future_2.GetCallback());
  EXPECT_TRUE(future_2.Wait());

  EXPECT_EQ(AuthenticatorStatus::PENDING_REQUEST, std::get<0>(future_2.Get()));
  EXPECT_FALSE(future_1.IsReady());
}

// Regression test for https://crbug.com/818219.
IN_PROC_BROWSER_TEST_F(WebAuthLocalClientBrowserTest,
                       GetPublicKeyCredentialTwiceInARow) {
  device::test::VirtualFidoDeviceFactory* virtual_device_factory =
      InjectVirtualFidoDeviceFactory();
  virtual_device_factory->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting(
          [&](device::VirtualFidoDevice* device) { return false; });

  TestGetFuture future_1;
  TestGetFuture future_2;
  authenticator()->GetAssertion(BuildBasicGetOptions(), future_1.GetCallback());
  authenticator()->GetAssertion(BuildBasicGetOptions(), future_2.GetCallback());
  EXPECT_TRUE(future_2.Wait());

  EXPECT_EQ(AuthenticatorStatus::PENDING_REQUEST, std::get<0>(future_2.Get()));
  EXPECT_FALSE(future_1.IsReady());
}

// WebAuthJavascriptClientBrowserTest -----------------------------------------

// Browser test fixture where the blink::mojom::Authenticator interface is
// normally accessed from Javascript in the renderer process.
class WebAuthJavascriptClientBrowserTest : public WebAuthBrowserTestBase {
 public:
  WebAuthJavascriptClientBrowserTest() = default;

  WebAuthJavascriptClientBrowserTest(
      const WebAuthJavascriptClientBrowserTest&) = delete;
  WebAuthJavascriptClientBrowserTest& operator=(
      const WebAuthJavascriptClientBrowserTest&) = delete;

  ~WebAuthJavascriptClientBrowserTest() override = default;

 private:
  // The "payment" extension tests require that SPC be enabled.
  const base::test::ScopedFeatureList scoped_feature_list_{
      features::kSecurePaymentConfirmation};
};

constexpr device::ProtocolVersion kAllProtocols[] = {
    device::ProtocolVersion::kCtap2, device::ProtocolVersion::kU2f};

// Tests that when navigator.credentials.create() is called with an invalid
// relying party id, we get a SecurityError.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       CreatePublicKeyCredentialInvalidRp) {
  CreateParameters parameters;
  parameters.rp_id = "localhost";
  std::string result = EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                              BuildCreateCallWithParameters(parameters))
                           .ExtractString();

  ASSERT_EQ(kRelyingPartySecurityErrorMessage,
            result.substr(0, strlen(kRelyingPartySecurityErrorMessage)));
}

// Tests that when navigator.credentials.create() is called with a null
// relying party, we get a NotSupportedError.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       CreatePublicKeyWithNullRp) {
  CreateParameters parameters;
  std::string script = BuildCreateCallWithParameters(parameters);
  const char kExpectedSubstr[] = "{ id: 'acme.com', name: 'Acme'}";
  const std::string::size_type offset = script.find(kExpectedSubstr);
  ASSERT_TRUE(offset != std::string::npos);
  script.replace(offset, sizeof(kExpectedSubstr) - 1, "null");

  std::string result =
      EvalJs(shell()->web_contents()->GetPrimaryMainFrame(), script)
          .ExtractString();
  ASSERT_EQ(kPublicKeyErrorMessage, result);
}

// Tests that when navigator.credentials.create() is called with user
// verification required, request times out.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       CreatePublicKeyCredentialWithUserVerification) {
  for (const auto protocol : kAllProtocols) {
    auto* virtual_device_factory = InjectVirtualFidoDeviceFactory();
    virtual_device_factory->SetSupportedProtocol(protocol);

    CreateParameters parameters;
    parameters.user_verification = "required";
    parameters.timeout = kShortTimeout;
    std::string result = EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                                BuildCreateCallWithParameters(parameters))
                             .ExtractString();
    ASSERT_EQ(kNotAllowedErrorMessage, result);
  }
}

// Tests that when navigator.credentials.create() is called with resident key
// required, request times out.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       CreatePublicKeyCredentialWithResidentKeyRequired) {
  for (const auto protocol : kAllProtocols) {
    auto* virtual_device_factory = InjectVirtualFidoDeviceFactory();
    virtual_device_factory->SetSupportedProtocol(protocol);

    CreateParameters parameters;
    parameters.require_resident_key = true;
    std::string result = EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                                BuildCreateCallWithParameters(parameters))
                             .ExtractString();

    ASSERT_EQ(kResidentCredentialsErrorMessage, result);
  }
}

// Tests that when navigator.credentials.create() is called with an
// unsupported algorithm, request times out.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       CreatePublicKeyCredentialAlgorithmNotSupported) {
  for (const auto protocol : kAllProtocols) {
    auto* virtual_device_factory = InjectVirtualFidoDeviceFactory();
    virtual_device_factory->SetSupportedProtocol(protocol);

    CreateParameters parameters;
    parameters.algorithm_identifier = "123";
    parameters.timeout = kShortTimeout;
    std::string result = EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                                BuildCreateCallWithParameters(parameters))
                             .ExtractString();

    ASSERT_EQ(kNotAllowedErrorMessage, result);
  }
}

// Tests that when navigator.credentials.create() is called with a
// platform authenticator requested, request times out.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       CreatePublicKeyCredentialPlatformAuthenticator) {
  for (const auto protocol : kAllProtocols) {
    auto* virtual_device_factory = InjectVirtualFidoDeviceFactory();
    virtual_device_factory->SetSupportedProtocol(protocol);

    CreateParameters parameters;
    parameters.authenticator_attachment = "platform";
    parameters.timeout = kShortTimeout;
    std::string result = EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                                BuildCreateCallWithParameters(parameters))
                             .ExtractString();

    ASSERT_EQ(kNotAllowedErrorMessage, result);
  }
}

// Tests that when navigator.credentials.create() is called with abort
// signal's aborted flag not set, we get a SUCCESS.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       CreatePublicKeyCredentialWithAbortNotSet) {
  for (const auto protocol : kAllProtocols) {
    auto* virtual_device_factory = InjectVirtualFidoDeviceFactory();
    virtual_device_factory->SetSupportedProtocol(protocol);

    CreateParameters parameters;
    parameters.signal = "authAbortSignal";
    std::string script =
        "authAbortController = new AbortController();"
        "authAbortSignal = authAbortController.signal;" +
        BuildCreateCallWithParameters(parameters);

    std::string result =
        EvalJs(shell()->web_contents()->GetPrimaryMainFrame(), script)
            .ExtractString();
    ASSERT_EQ(kOkMessage, result);
  }
}

// Tests that when navigator.credentials.create() is called with abort
// signal's aborted flag set before sending request, we get an AbortError.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       CreatePublicKeyCredentialWithAbortSetBeforeCreate) {
  CreateParameters parameters;
  parameters.signal = "authAbortSignal";
  std::string script =
      "authAbortController = new AbortController();"
      "authAbortSignal = authAbortController.signal;"
      "authAbortController.abort();" +
      BuildCreateCallWithParameters(parameters);

  std::string result =
      EvalJs(shell()->web_contents()->GetPrimaryMainFrame(), script)
          .ExtractString();
  ASSERT_EQ(kAbortErrorMessage, result.substr(0, strlen(kAbortErrorMessage)));
}

// Tests that when navigator.credentials.create() is called with abort
// signal's aborted flag set with reason before sending request,
// we get an error from the reason.
IN_PROC_BROWSER_TEST_F(
    WebAuthJavascriptClientBrowserTest,
    CreatePublicKeyCredentialWithAbortSetWithReasonBeforeCreate) {
  CreateParameters parameters;
  parameters.signal = "authAbortSignal";
  std::string script =
      "authAbortController = new AbortController();"
      "authAbortSignal = authAbortController.signal;"
      "authAbortController.abort('Error');" +
      BuildCreateCallWithParameters(parameters);

  std::string result =
      EvalJs(shell()->web_contents()->GetPrimaryMainFrame(), script)
          .ExtractString();
  ASSERT_EQ(kAbortReasonMessage, result.substr(0, strlen(kAbortReasonMessage)));
}

// Tests that when navigator.credentials.create() is called with abort
// signal's aborted flag set after sending request, we get an AbortError.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       CreatePublicKeyCredentialWithAbortSetAfterCreate) {
  // This test sends the abort signal after making the WebAuthn call. However,
  // the WebAuthn call could complete before the abort signal is sent, leading
  // to a flakey test. Thus the |simulate_press_callback| is installed and
  // always returns false, to ensure that the VirtualFidoDevice stalls the
  // WebAuthn call and the abort signal will happen in time.
  device::test::VirtualFidoDeviceFactory* virtual_device_factory =
      InjectVirtualFidoDeviceFactory();
  virtual_device_factory->mutable_state()->simulate_press_callback =
      base::BindRepeating(
          [](device::VirtualFidoDevice*) -> bool { return false; });

  CreateParameters parameters;
  parameters.signal = "authAbortSignal";
  std::string script =
      "authAbortController = new AbortController();"
      "authAbortSignal = authAbortController.signal;"
      "const promise = " +
      BuildCreateCallWithParameters(parameters) +
      ";"
      "authAbortController.abort();"
      "promise;";

  std::string result =
      EvalJs(shell()->web_contents()->GetPrimaryMainFrame(), script)
          .ExtractString();
  ASSERT_EQ(kAbortErrorMessage, result.substr(0, strlen(kAbortErrorMessage)));
}

// Tests that when navigator.credentials.create() is called with abort
// signal's aborted flag set with reason after sending request, we get an error
// from the reason.
IN_PROC_BROWSER_TEST_F(
    WebAuthJavascriptClientBrowserTest,
    CreatePublicKeyCredentialWithAbortSetWithReasonAfterCreate) {
  // This test sends the abort signal after making the WebAuthn call. However,
  // the WebAuthn call could complete before the abort signal is sent, leading
  // to a flakey test. Thus the |simulate_press_callback| is installed and
  // always returns false, to ensure that the VirtualFidoDevice stalls the
  // WebAuthn call and the abort signal will happen in time.
  device::test::VirtualFidoDeviceFactory* virtual_device_factory =
      InjectVirtualFidoDeviceFactory();
  virtual_device_factory->mutable_state()->simulate_press_callback =
      base::BindRepeating(
          [](device::VirtualFidoDevice*) -> bool { return false; });

  CreateParameters parameters;
  parameters.signal = "authAbortSignal";
  std::string script =
      "authAbortController = new AbortController();"
      "authAbortSignal = authAbortController.signal;"
      "const promise = " +
      BuildCreateCallWithParameters(parameters) +
      ";"
      "authAbortController.abort('Error');"
      "promise;";

  std::string result =
      EvalJs(shell()->web_contents()->GetPrimaryMainFrame(), script)
          .ExtractString();
  ASSERT_EQ(kAbortReasonMessage, result.substr(0, strlen(kAbortReasonMessage)));
}

// Tests that when navigator.credentials.get() is called with user verification
// required, we get an NotAllowedError because the virtual device isn't
// configured with UV and GetAssertionRequestHandler will return
// |kAuthenticatorMissingUserVerification| when such an authenticator is
// touched in that case.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       GetPublicKeyCredentialUserVerification) {
  for (const auto protocol : kAllProtocols) {
    auto* virtual_device_factory = InjectVirtualFidoDeviceFactory();
    virtual_device_factory->SetSupportedProtocol(protocol);

    GetParameters parameters;
    parameters.user_verification = "required";
    std::string result = EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                                BuildGetCallWithParameters(parameters))
                             .ExtractString();
    ASSERT_EQ(kNotAllowedErrorMessage, result);
  }
}

// Test that unknown transport types are ignored.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       UnknownTransportType) {
  auto* virtual_device_factory = InjectVirtualFidoDeviceFactory();
  virtual_device_factory->SetSupportedProtocol(device::ProtocolVersion::kCtap2);

  GetParameters parameters;
  parameters.allow_credentials =
      "[{"
      "  type: 'public-key',"
      "  id: new TextEncoder().encode('allowedCredential'),"
      "  transports: ['carrierpigeon'],"
      "}]";
  parameters.timeout = kShortTimeout;
  std::string result = EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                              BuildGetCallWithParameters(parameters))
                           .ExtractString();
  ASSERT_EQ(kNotAllowedErrorMessage, result);
}

IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest, HybridRecognised) {
  // Ensure that both "cable" and "hybrid" are recognised as the same transport.
  auto* virtual_device_factory = InjectVirtualFidoDeviceFactory();
  virtual_device_factory->SetTransport(device::FidoTransportProtocol::kHybrid);
  virtual_device_factory->SetSupportedProtocol(device::ProtocolVersion::kCtap2);
  static const uint8_t kCredentialId[] = {1};
  ASSERT_TRUE(virtual_device_factory->mutable_state()->InjectRegistration(
      kCredentialId, "acme.com"));

  GetParameters parameters;
  for (const char* const transport_str : {"hybrid", "cable", "usb"}) {
    SCOPED_TRACE(transport_str);
    const bool should_fail = (strcmp(transport_str, "usb") == 0);

    parameters.allow_credentials =
        "[{"
        "  type: 'public-key',"
        "  id: new Uint8Array([1]),"
        "  transports: ['" +
        std::string(transport_str) +
        "'],"
        "}]";
    if (should_fail) {
      parameters.timeout = kShortTimeout;
    }
    std::string result = EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                                BuildGetCallWithParameters(parameters))
                             .ExtractString();

    if (should_fail) {
      ASSERT_EQ(kNotAllowedErrorMessage, result);
    } else {
      ASSERT_EQ(kOkMessage, result);
    }
  }
}

// Tests that when navigator.credentials.get() is called with an empty
// allowCredentials list, we get a NotSupportedError.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       GetPublicKeyCredentialEmptyAllowCredentialsList) {
  InjectVirtualFidoDeviceFactory();
  GetParameters parameters;
  parameters.allow_credentials = "[]";
  std::string result = EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                              BuildGetCallWithParameters(parameters))
                           .ExtractString();
  ASSERT_EQ(kResidentCredentialsErrorMessage, result);
}

// Tests that when navigator.credentials.get() is called with abort
// signal's aborted flag not set, we get a NOT_ALLOWED_ERROR, because the
// virtual device does not have any registered credentials.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       GetPublicKeyCredentialWithAbortNotSet) {
  for (const auto protocol : kAllProtocols) {
    auto* virtual_device_factory = InjectVirtualFidoDeviceFactory();
    virtual_device_factory->SetSupportedProtocol(protocol);

    GetParameters parameters;
    parameters.signal = "authAbortSignal";
    std::string script =
        "authAbortController = new AbortController();"
        "authAbortSignal = authAbortController.signal;" +
        BuildGetCallWithParameters(parameters);

    std::string result =
        EvalJs(shell()->web_contents()->GetPrimaryMainFrame(), script)
            .ExtractString();
    ASSERT_EQ(kNotAllowedErrorMessage, result);
  }
}

// Tests that when navigator.credentials.get() is called with abort
// signal's aborted flag set before sending request, we get an AbortError.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       GetPublicKeyCredentialWithAbortSetBeforeGet) {
  GetParameters parameters;
  parameters.signal = "authAbortSignal";
  std::string script =
      "authAbortController = new AbortController();"
      "authAbortSignal = authAbortController.signal;"
      "authAbortController.abort();" +
      BuildGetCallWithParameters(parameters);

  std::string result =
      EvalJs(shell()->web_contents()->GetPrimaryMainFrame(), script)
          .ExtractString();
  ASSERT_EQ(kAbortErrorMessage, result.substr(0, strlen(kAbortErrorMessage)));
}

// Tests that when navigator.credentials.get() is called with abort
// signal's aborted flag set with reason before sending request,
// we get an error from the reason.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       GetPublicKeyCredentialWithAbortSetWithReasonBeforeGet) {
  GetParameters parameters;
  parameters.signal = "authAbortSignal";
  std::string script =
      "authAbortController = new AbortController();"
      "authAbortSignal = authAbortController.signal;"
      "authAbortController.abort('Error');" +
      BuildGetCallWithParameters(parameters);

  std::string result =
      EvalJs(shell()->web_contents()->GetPrimaryMainFrame(), script)
          .ExtractString();
  ASSERT_EQ(kAbortReasonMessage, result.substr(0, strlen(kAbortReasonMessage)));
}

// Tests that when navigator.credentials.get() is called with abort
// signal's aborted flag set after sending request, we get an AbortError.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       GetPublicKeyCredentialWithAbortSetAfterGet) {
  // This test sends the abort signal after making the WebAuthn call. However,
  // the WebAuthn call could complete before the abort signal is sent, leading
  // to a flakey test. Thus the |simulate_press_callback| is installed and
  // always returns false, to ensure that the VirtualFidoDevice stalls the
  // WebAuthn call and the abort signal will happen in time.
  device::test::VirtualFidoDeviceFactory* virtual_device_factory =
      InjectVirtualFidoDeviceFactory();
  virtual_device_factory->mutable_state()->simulate_press_callback =
      base::BindRepeating(
          [](device::VirtualFidoDevice*) -> bool { return false; });

  GetParameters parameters;
  parameters.signal = "authAbortSignal";
  std::string script =
      "authAbortController = new AbortController();"
      "authAbortSignal = authAbortController.signal;"
      "const promise = " +
      BuildGetCallWithParameters(parameters) +
      ";"
      "authAbortController.abort();"
      "promise";

  std::string result =
      EvalJs(shell()->web_contents()->GetPrimaryMainFrame(), script)
          .ExtractString();
  ASSERT_EQ(kAbortErrorMessage, result.substr(0, strlen(kAbortErrorMessage)));
}

// Tests that when navigator.credentials.get() is called with abort
// signal's aborted flag set with reason after sending request,
// we get an error from the reason.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       GetPublicKeyCredentialWithAbortSetWithReasonAfterGet) {
  // This test sends the abort signal after making the WebAuthn call. However,
  // the WebAuthn call could complete before the abort signal is sent, leading
  // to a flakey test. Thus the |simulate_press_callback| is installed and
  // always returns false, to ensure that the VirtualFidoDevice stalls the
  // WebAuthn call and the abort signal will happen in time.
  device::test::VirtualFidoDeviceFactory* virtual_device_factory =
      InjectVirtualFidoDeviceFactory();
  virtual_device_factory->mutable_state()->simulate_press_callback =
      base::BindRepeating(
          [](device::VirtualFidoDevice*) -> bool { return false; });

  GetParameters parameters;
  parameters.signal = "authAbortSignal";
  std::string script =
      "authAbortController = new AbortController();"
      "authAbortSignal = authAbortController.signal;"
      "const promise = " +
      BuildGetCallWithParameters(parameters) +
      ";"
      "authAbortController.abort('Error');"
      "promise;";

  std::string result =
      EvalJs(shell()->web_contents()->GetPrimaryMainFrame(), script)
          .ExtractString();
  ASSERT_EQ(kAbortReasonMessage, result.substr(0, strlen(kAbortReasonMessage)));
}

IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       RequestsFromIFrames) {
  static constexpr char kOuterHost[] = "acme.com";
  static constexpr char kInnerHost[] = "notacme.com";
  EXPECT_TRUE(NavigateToURL(shell(),
                            GetHttpsURL(kOuterHost, "/page_with_iframe.html")));

  auto* virtual_device_factory = InjectVirtualFidoDeviceFactory();
  static constexpr uint8_t kOuterCredentialID = 1;
  static constexpr uint8_t kOuterCredentialIDArray[] = {kOuterCredentialID};
  static constexpr uint8_t kInnerCredentialID = 2;
  static constexpr uint8_t kInnerCredentialIDArray[] = {kInnerCredentialID};
  ASSERT_TRUE(virtual_device_factory->mutable_state()->InjectRegistration(
      kOuterCredentialIDArray, kOuterHost));
  ASSERT_TRUE(virtual_device_factory->mutable_state()->InjectRegistration(
      kInnerCredentialIDArray, kInnerHost));

  static constexpr struct kTestCase {
    // Whether the iframe loads from a different origin.
    bool cross_origin;
    bool create_should_work;
    bool get_should_work;
    // The contents of an "allow" attribute on the iframe.
    const char allow_value[32];
  } kTestCases[] = {
      // XO |Create|Get  | Allow
      {false, true, true, ""},
      {true, false, false, ""},
      {true, false, true, "publickey-credentials-get"},
      {true, true, false, "publickey-credentials-create"},
      // The "payment" policy works for payment extension credentials (see
      // `PaymentCredentialCreationFromIFrames`), but should not for other
      // WebAuthn credentials.
      {true, false, false, "payment"},
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.allow_value);
    SCOPED_TRACE(test.cross_origin);

    const std::string setAllowJS = base::StringPrintf(
        "document.getElementById('test_iframe').setAttribute('allow', '%s'); "
        "'OK';",
        test.allow_value);
    ASSERT_EQ("OK", EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                           setAllowJS.c_str()));

    if (test.cross_origin) {
      // Create a cross-origin iframe by loading it from notacme.com.
      NavigateIframeToURL(shell()->web_contents(), "test_iframe",
                          GetHttpsURL(kInnerHost, "/title2.html"));
    } else {
      // Create a same-origin iframe by loading it from acme.com.
      NavigateIframeToURL(shell()->web_contents(), "test_iframe",
                          GetHttpsURL(kOuterHost, "/title2.html"));
    }

    RenderFrameHost* const iframe = ChildFrameAt(shell()->web_contents(), 0);
    ASSERT_TRUE(iframe);

    CreateParameters create_parameters;
    create_parameters.rp_id = test.cross_origin ? "notacme.com" : "acme.com";
    std::string result =
        EvalJs(iframe, BuildCreateCallWithParameters(create_parameters))
            .ExtractString();
    if (test.create_should_work) {
      EXPECT_EQ(std::string(kOkMessage), result);
    } else {
      EXPECT_EQ(kCreatePermissionsPolicyMissingMessage, result);
    }

    GetParameters get_params;
    const int credential_id =
        test.cross_origin ? kInnerCredentialID : kOuterCredentialID;
    get_params.rp_id = create_parameters.rp_id;
    get_params.allow_credentials = base::StringPrintf(
        "[{ type: 'public-key',"
        "   id: new Uint8Array([%d]),"
        "}]",
        credential_id);
    result =
        EvalJs(iframe, BuildGetCallWithParameters(get_params)).ExtractString();
    if (test.get_should_work) {
      EXPECT_EQ(std::string(kOkMessage), result);
    } else {
      EXPECT_EQ(kGetPermissionsPolicyMissingMessage, result);
    }
  }
}

IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       PaymentCredentialCreationFromIFrames) {
  // This call is necessary for WebAuthenticationDelegate::SupportsResidentKeys
  // to return true.
  content::AuthenticatorEnvironment::GetInstance()
      ->EnableVirtualAuthenticatorFor(
          static_cast<content::RenderFrameHostImpl*>(
              shell()->web_contents()->GetPrimaryMainFrame())
              ->frame_tree_node(),
          /*enable_ui=*/false);

  static constexpr char kOuterHost[] = "acme.com";
  static constexpr char kInnerHost[] = "notacme.com";
  EXPECT_TRUE(NavigateToURL(shell(),
                            GetHttpsURL(kOuterHost, "/page_with_iframe.html")));

  // SPC credentials (i.e., credentials with the "payment" extension specified)
  // require a platform authenticator that supports resident keys.
  auto* virtual_device_factory = InjectVirtualFidoDeviceFactory();
  virtual_device_factory->SetTransport(
      device::FidoTransportProtocol::kInternal);
  virtual_device_factory->SetSupportedProtocol(device::ProtocolVersion::kCtap2);
  virtual_device_factory->mutable_state()->fingerprints_enrolled = true;
  device::VirtualCtap2Device::Config config;
  config.resident_key_support = true;
  config.is_platform_authenticator = true;
  config.internal_uv_support = true;
  virtual_device_factory->SetCtap2Config(config);

  static constexpr struct kTestCase {
    // Whether the iframe loads from a different origin.
    bool cross_origin;
    bool create_with_payment_should_work;
    // The contents of an "allow" attribute on the iframe.
    const char allow_value[32];
  } kTestCases[] = {
      // XO |CreateWithPayment |Allow
      {false, true, ""},
      {true, false, ""},
      {true, false, "publickey-credentials-get"},
      {true, true, "publickey-credentials-create"},
      {true, true, "payment"},
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.allow_value);
    SCOPED_TRACE(test.cross_origin);

    const std::string setAllowJS = base::StringPrintf(
        "document.getElementById('test_iframe').setAttribute('allow', '%s'); "
        "'OK';",
        test.allow_value);
    ASSERT_EQ("OK", EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                           setAllowJS.c_str()));

    if (test.cross_origin) {
      // Create a cross-origin iframe by loading it from notacme.com.
      NavigateIframeToURL(shell()->web_contents(), "test_iframe",
                          GetHttpsURL(kInnerHost, "/title2.html"));
    } else {
      // Create a same-origin iframe by loading it from acme.com.
      NavigateIframeToURL(shell()->web_contents(), "test_iframe",
                          GetHttpsURL(kOuterHost, "/title2.html"));
    }

    RenderFrameHost* const iframe = ChildFrameAt(shell()->web_contents(), 0);
    ASSERT_TRUE(iframe);

    CreateParameters create_parameters;
    create_parameters.rp_id = test.cross_origin ? "notacme.com" : "acme.com";
    create_parameters.require_resident_key = true;
    create_parameters.user_verification = "required";
    create_parameters.authenticator_attachment = "platform";
    create_parameters.is_payment = true;
    std::string result =
        EvalJs(iframe, BuildCreateCallWithParameters(create_parameters))
            .ExtractString();
    if (test.create_with_payment_should_work) {
      EXPECT_EQ(std::string(kOkMessage), result);
    } else {
      EXPECT_EQ(kCreateWithPaymentPermissionsPolicyMissingMessage, result);
    }
  }
}

// Tests that a credentials.create() call triggered by the main frame will
// successfully complete even if a subframe navigation takes place while the
// request is waiting for user consent.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       NavigateSubframeDuringPress) {
  auto* virtual_device_factory = InjectVirtualFidoDeviceFactory();
  bool prompt_callback_was_invoked = false;
  virtual_device_factory->mutable_state()->simulate_press_callback =
      base::BindLambdaForTesting([&](device::VirtualFidoDevice* device) {
        prompt_callback_was_invoked = true;
        NavigateIframeToURL(shell()->web_contents(), "test_iframe",
                            GURL("/title2.html"));
        return true;
      });

  EXPECT_TRUE(NavigateToURL(
      shell(), GetHttpsURL("www.acme.com", "/page_with_iframe.html")));

  ASSERT_EQ(kOkMessage,
            EvalJs(shell()->web_contents(),
                   BuildCreateCallWithParameters(CreateParameters())));
  ASSERT_TRUE(prompt_callback_was_invoked);
}

IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       BadCableExtensionVersions) {
  // The caBLE extension should only contain v1 data. Test that nothing crashes
  // if a site tries to set other versions.

  InjectVirtualFidoDeviceFactory();
  GetParameters parameters;
  parameters.allow_credentials =
      "[{ type: 'public-key',"
      "  id: new TextEncoder().encode('allowedCredential'),"
      "  transports: ['cable']}],"
      "extensions: {"
      "  cableAuthentication: [{"
      "    version: 1,"
      "    clientEid: new Uint8Array(Array(16).fill(1)),"
      "    authenticatorEid: new Uint8Array(Array(16).fill(2)),"
      "    sessionPreKey: new Uint8Array(Array(32).fill(3)),"
      "  },{"
      "    version: 2,"
      "    clientEid: new Uint8Array(Array(16).fill(1)),"
      "    authenticatorEid: new Uint8Array(Array(16).fill(2)),"
      "    sessionPreKey: new Uint8Array(Array(32).fill(3)),"
      "  },{"
      "    version: 3,"
      "    clientEid: new Uint8Array(Array(16).fill(1)),"
      "    authenticatorEid: new Uint8Array(Array(16).fill(2)),"
      "    sessionPreKey: new Uint8Array(Array(32).fill(3)),"
      "  }]"
      "}";
  ASSERT_EQ(kNotAllowedErrorMessage,
            EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                   BuildGetCallWithParameters(parameters)));
}

#if BUILDFLAG(IS_WIN)
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest, WinMakeCredential) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GetHttpsURL("www.acme.com", "/title1.html")));

  device::FakeWinWebAuthnApi fake_api;
  fake_api.set_is_uvpaa(true);
  device::WinWebAuthnApi::ScopedOverride win_webauthn_api_override(&fake_api);

  ASSERT_EQ(kOkMessage,
            EvalJs(shell()->web_contents(),
                   BuildCreateCallWithParameters(CreateParameters())));
}

IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       WinMakeCredentialReturnCodeFailure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GetHttpsURL("www.acme.com", "/title1.html")));
  device::FakeWinWebAuthnApi fake_api;
  device::WinWebAuthnApi::ScopedOverride win_webauthn_api_override(&fake_api);

  // Errors documented for WebAuthNGetErrorName() in <webauthn.h>.
  const std::map<HRESULT, std::string> errors{
      // NTE_EXISTS is the error for using an authenticator that matches the
      // exclude list, which should result in "InvalidStateError".
      {NTE_EXISTS, kInvalidStateErrorMessage},
      // All other errors should yield "NotAllowedError".
      {HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED), kNotAllowedErrorMessage},
      {NTE_TOKEN_KEYSET_STORAGE_FULL, kNotAllowedErrorMessage},
      {NTE_TOKEN_KEYSET_STORAGE_FULL, kNotAllowedErrorMessage},
      {NTE_INVALID_PARAMETER, kNotAllowedErrorMessage},
      {NTE_DEVICE_NOT_FOUND, kNotAllowedErrorMessage},
      {NTE_NOT_FOUND, kNotAllowedErrorMessage},
      {HRESULT_FROM_WIN32(ERROR_CANCELLED), kNotAllowedErrorMessage},
      {NTE_USER_CANCELLED, kNotAllowedErrorMessage},
      {HRESULT_FROM_WIN32(ERROR_TIMEOUT), kNotAllowedErrorMessage},
      // Undocumented errors should default to NOT_ALLOWED_ERROR.
      {ERROR_FILE_NOT_FOUND, kNotAllowedErrorMessage},
  };

  for (const auto& error : errors) {
    fake_api.set_hresult(error.first);

    EXPECT_EQ(error.second,
              EvalJs(shell()->web_contents(),
                     BuildCreateCallWithParameters(CreateParameters())));
  }
}

IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest, WinGetAssertion) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GetHttpsURL("www.acme.com", "/title1.html")));

  constexpr uint8_t credential_id[] = {'A', 'A', 'A'};

  device::FakeWinWebAuthnApi fake_api;
  fake_api.InjectNonDiscoverableCredential(credential_id, "acme.com");
  device::WinWebAuthnApi::ScopedOverride win_webauthn_api_override(&fake_api);

  GetParameters get_parameters;
  get_parameters.allow_credentials =
      "[{ type: 'public-key', id: new TextEncoder().encode('AAA')}]";

  ASSERT_EQ(kOkMessage, EvalJs(shell()->web_contents(),
                               BuildGetCallWithParameters(get_parameters)));
}

IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       WinGetAssertionReturnCodeFailure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GetHttpsURL("www.acme.com", "/title1.html")));
  device::FakeWinWebAuthnApi fake_api;
  device::WinWebAuthnApi::ScopedOverride win_webauthn_api_override(&fake_api);

  // Errors documented for WebAuthNGetErrorName() in <webauthn.h>.
  const std::set<HRESULT> errors{
      // NTE_EXISTS -- should not be returned for WebAuthNGetAssertion().
      HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED), NTE_TOKEN_KEYSET_STORAGE_FULL,
      NTE_TOKEN_KEYSET_STORAGE_FULL, NTE_INVALID_PARAMETER,
      NTE_DEVICE_NOT_FOUND, NTE_NOT_FOUND, HRESULT_FROM_WIN32(ERROR_CANCELLED),
      NTE_USER_CANCELLED, HRESULT_FROM_WIN32(ERROR_TIMEOUT),
      // Other errors should also result in NOT_ALLOWED_ERROR.
      ERROR_FILE_NOT_FOUND};

  for (const auto& error : errors) {
    fake_api.set_hresult(error);

    ASSERT_EQ(kNotAllowedErrorMessage,
              EvalJs(shell()->web_contents(),
                     BuildGetCallWithParameters(GetParameters())));
  }
}
#endif

IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       GetAssertionOversizedAllowList) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GetHttpsURL("www.acme.com", "/title1.html")));

  GetParameters get_parameters;
  get_parameters.allow_credentials =
      "Array(65).fill({ type: 'public-key', id: new "
      "TextEncoder().encode('A')})";

  ASSERT_EQ(kAllowCredentialsRangeErrorMessage,
            EvalJs(shell()->web_contents(),
                   BuildGetCallWithParameters(get_parameters)));
}

IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       MakeCredentialOversizedExcludeList) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GetHttpsURL("www.acme.com", "/title1.html")));

  CreateParameters parameters;
  parameters.exclude_credentials =
      "Array(65).fill({type: 'public-key', id: new TextEncoder().encode('A')})";

  ASSERT_EQ(kExcludeCredentialsRangeErrorMessage,
            EvalJs(shell()->web_contents(),
                   BuildCreateCallWithParameters(parameters)));
}

class WebAuthCrossDomainTest : public WebAuthBrowserTestBase {
 public:
  void SetUpOnMainThread() override {
    WebAuthBrowserTestBase::SetUpOnMainThread();

    virtual_device_factory_ = InjectVirtualFidoDeviceFactory();
    virtual_device_factory_->SetTransport(
        device::FidoTransportProtocol::kUsbHumanInterfaceDevice);
    virtual_device_factory_->SetSupportedProtocol(
        device::ProtocolVersion::kCtap2);
  }

 protected:
  raw_ptr<device::test::VirtualFidoDeviceFactory> virtual_device_factory_;
  const base::test::ScopedFeatureList scoped_feature_list{
      device::kWebAuthnRelatedOrigin};
};

IN_PROC_BROWSER_TEST_F(WebAuthCrossDomainTest, Create) {
  CreateParameters parameters;
  parameters.rp_id = "foo.com";
  test_client()->set_webauthn_origins_response(
      "application/json", GetHttpsURL("www.acme.com", "/").spec());
  std::string result = EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                              BuildCreateCallWithParameters(parameters))
                           .ExtractString();

  EXPECT_EQ(kOkMessage, result);
}

IN_PROC_BROWSER_TEST_F(WebAuthCrossDomainTest, CreateBadContentType) {
  CreateParameters parameters;
  parameters.rp_id = "foo.com";
  test_client()->set_webauthn_origins_response(
      "text/plain", GetHttpsURL("www.acme.com", "/").spec());
  std::string result = EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                              BuildCreateCallWithParameters(parameters))
                           .ExtractString();

  EXPECT_EQ(kRpIdContentTypeMessage, result);
}

IN_PROC_BROWSER_TEST_F(WebAuthCrossDomainTest, CreateBadOrigin) {
  CreateParameters parameters;
  parameters.rp_id = "foo.com";
  test_client()->set_webauthn_origins_response("application/json",
                                               "https://nottherightdomain.com");
  std::string result = EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                              BuildCreateCallWithParameters(parameters))
                           .ExtractString();

  EXPECT_EQ(kRpIdNoEntryMessage, result);
}

IN_PROC_BROWSER_TEST_F(WebAuthCrossDomainTest, Timeout) {
  // Have the request timeout happen while RP ID validation is pending.
  CreateParameters parameters;
  parameters.rp_id = "foo.com";
  parameters.timeout = kShortTimeout;
  test_client()->sinkhole_webauthn_origins_requests();
  std::string result = EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                              BuildCreateCallWithParameters(parameters))
                           .ExtractString();

  EXPECT_EQ(kNotAllowedErrorMessage, result);
}

IN_PROC_BROWSER_TEST_F(WebAuthCrossDomainTest, Get) {
  const uint8_t kCredentialId[] = {0x61, 0x6C, 0x6C, 0x6F, 0x77, 0x65,
                                   0x64, 0x43, 0x72, 0x65, 0x64, 0x65,
                                   0x6E, 0x74, 0x69, 0x61, 0x6C};
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      device::fido_parsing_utils::Materialize(base::make_span(kCredentialId)),
      "foo.com"));

  GetParameters parameters;
  parameters.user_verification = "discouraged";
  parameters.rp_id = "foo.com";
  test_client()->set_webauthn_origins_response(
      "application/json", GetHttpsURL("www.acme.com", "/").spec());
  std::string result = EvalJs(shell()->web_contents()->GetPrimaryMainFrame(),
                              BuildGetCallWithParameters(parameters))
                           .ExtractString();
  ASSERT_EQ(kOkMessage, result);
}

class WebAuthLocalClientBackForwardCacheBrowserTest
    : public WebAuthLocalClientBrowserTest {
 protected:
  BackForwardCacheDisabledTester tester_;
};

IN_PROC_BROWSER_TEST_F(WebAuthLocalClientBackForwardCacheBrowserTest,
                       WebAuthDisablesBackForwardCache) {
  // Initialisation of the test should disable bfcache.
  EXPECT_TRUE(tester_.IsDisabledForFrameWithReason(
      shell()->web_contents()->GetPrimaryMainFrame()->GetProcess()->GetID(),
      shell()->web_contents()->GetPrimaryMainFrame()->GetRoutingID(),
      BackForwardCacheDisable::DisabledReason(
          BackForwardCacheDisable::DisabledReasonId::kWebAuthenticationAPI)));
}

// WebAuthBrowserCtapTest ----------------------------------------------

class WebAuthBrowserCtapTest : public WebAuthLocalClientBrowserTest {
 public:
  WebAuthBrowserCtapTest() = default;

  WebAuthBrowserCtapTest(const WebAuthBrowserCtapTest&) = delete;
  WebAuthBrowserCtapTest& operator=(const WebAuthBrowserCtapTest&) = delete;

  ~WebAuthBrowserCtapTest() override = default;
};

IN_PROC_BROWSER_TEST_F(WebAuthBrowserCtapTest, TestMakeCredential) {
  for (const auto protocol : kAllProtocols) {
    auto* virtual_device_factory = InjectVirtualFidoDeviceFactory();
    virtual_device_factory->SetSupportedProtocol(protocol);

    TestCreateFuture create_future;
    authenticator()->MakeCredential(BuildBasicCreateOptions(),
                                    create_future.GetCallback());

    EXPECT_TRUE(create_future.Wait());
    EXPECT_EQ(AuthenticatorStatus::SUCCESS, std::get<0>(create_future.Get()));
  }
}

IN_PROC_BROWSER_TEST_F(WebAuthBrowserCtapTest,
                       TestMakeCredentialWithDuplicateKeyHandle) {
  for (const auto protocol : kAllProtocols) {
    auto* virtual_device_factory = InjectVirtualFidoDeviceFactory();
    virtual_device_factory->SetSupportedProtocol(protocol);
    auto make_credential_request = BuildBasicCreateOptions();
    device::PublicKeyCredentialDescriptor excluded_credential(
        device::CredentialType::kPublicKey,
        device::fido_parsing_utils::Materialize(
            device::test_data::kCtap2MakeCredentialCredentialId),
        std::vector<device::FidoTransportProtocol>{
            device::FidoTransportProtocol::kUsbHumanInterfaceDevice});
    make_credential_request->exclude_credentials.push_back(excluded_credential);

    ASSERT_TRUE(virtual_device_factory->mutable_state()->InjectRegistration(
        device::fido_parsing_utils::Materialize(
            device::test_data::kCtap2MakeCredentialCredentialId),
        make_credential_request->relying_party.id));

    TestCreateFuture create_future;
    authenticator()->MakeCredential(std::move(make_credential_request),
                                    create_future.GetCallback());

    EXPECT_TRUE(create_future.Wait());
    EXPECT_EQ(AuthenticatorStatus::CREDENTIAL_EXCLUDED,
              std::get<0>(create_future.Get()));
  }
}

IN_PROC_BROWSER_TEST_F(WebAuthBrowserCtapTest, TestGetAssertion) {
  for (const auto protocol : kAllProtocols) {
    auto* virtual_device_factory = InjectVirtualFidoDeviceFactory();
    virtual_device_factory->SetSupportedProtocol(protocol);
    auto get_assertion_request_params = BuildBasicGetOptions();
    ASSERT_TRUE(virtual_device_factory->mutable_state()->InjectRegistration(
        device::fido_parsing_utils::Materialize(
            device::test_data::kTestGetAssertionCredentialId),
        get_assertion_request_params->relying_party_id));

    TestGetFuture get_future;
    authenticator()->GetAssertion(std::move(get_assertion_request_params),
                                  get_future.GetCallback());
    EXPECT_TRUE(get_future.Wait());
    EXPECT_EQ(AuthenticatorStatus::SUCCESS, std::get<0>(get_future.Get()));
  }
}

IN_PROC_BROWSER_TEST_F(WebAuthBrowserCtapTest,
                       TestGetAssertionWithNoMatchingKeyHandles) {
  for (const auto protocol : kAllProtocols) {
    auto* virtual_device_factory = InjectVirtualFidoDeviceFactory();
    virtual_device_factory->SetSupportedProtocol(protocol);
    auto get_assertion_request_params = BuildBasicGetOptions();

    TestGetFuture get_future;
    authenticator()->GetAssertion(std::move(get_assertion_request_params),
                                  get_future.GetCallback());
    EXPECT_TRUE(get_future.Wait());
    EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR,
              std::get<0>(get_future.Get()));
  }
}

// Helper test class to track expected invocations of
// `WebContentsObserver::WebAuthnAssertionRequestSucceeded()` via `std::string`
// logs.
class WCOCallbackLogger : public WebContentsObserver,
                          public WebContentsUserData<WCOCallbackLogger> {
 public:
  WCOCallbackLogger(const WCOCallbackLogger&) = delete;
  WCOCallbackLogger& operator=(const WCOCallbackLogger&) = delete;

  const std::vector<std::string>& log() const { return log_; }

  // Start WebContentsObserver overrides:
  void WebAuthnAssertionRequestSucceeded(
      content::RenderFrameHost* render_frame_host) override {
    log_.push_back(render_frame_host->GetLastCommittedURL().host());
  }
  // End WebContentsObserver overrides.

 private:
  explicit WCOCallbackLogger(content::WebContents* web_contents)
      : WebContentsObserver(web_contents),
        content::WebContentsUserData<WCOCallbackLogger>(*web_contents) {}
  // So WebContentsUserData::CreateForWebContents() can call the constructor.
  friend class content::WebContentsUserData<WCOCallbackLogger>;

  std::vector<std::string> log_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(WCOCallbackLogger);

IN_PROC_BROWSER_TEST_F(WebAuthBrowserCtapTest,
                       SuccessfulAssertion_ConfirmWCOCallback) {
  WCOCallbackLogger::CreateForWebContents(shell()->web_contents());
  auto* logger = WCOCallbackLogger::FromWebContents(shell()->web_contents());

  device::test::VirtualFidoDeviceFactory* virtual_device_factory =
      InjectVirtualFidoDeviceFactory();
  virtual_device_factory->SetSupportedProtocol(device::ProtocolVersion::kCtap2);
  blink::mojom::PublicKeyCredentialRequestOptionsPtr
      get_assertion_request_params = BuildBasicGetOptions();
  ASSERT_TRUE(virtual_device_factory->mutable_state()->InjectRegistration(
      device::fido_parsing_utils::Materialize(
          device::test_data::kTestGetAssertionCredentialId),
      get_assertion_request_params->relying_party_id));

  TestGetFuture get_future;
  authenticator()->GetAssertion(std::move(get_assertion_request_params),
                                get_future.GetCallback());
  EXPECT_TRUE(get_future.Wait());
  EXPECT_EQ(AuthenticatorStatus::SUCCESS, std::get<0>(get_future.Get()));

  EXPECT_THAT(logger->log(), testing::ElementsAre("www.acme.com"));
}

IN_PROC_BROWSER_TEST_F(WebAuthBrowserCtapTest,
                       UnsuccessfulAssertion_ConfirmNoWCOCallback) {
  WCOCallbackLogger::CreateForWebContents(shell()->web_contents());
  auto* logger = WCOCallbackLogger::FromWebContents(shell()->web_contents());

  device::test::VirtualFidoDeviceFactory* virtual_device_factory =
      InjectVirtualFidoDeviceFactory();
  virtual_device_factory->SetSupportedProtocol(device::ProtocolVersion::kCtap2);
  blink::mojom::PublicKeyCredentialRequestOptionsPtr
      get_assertion_request_params = BuildBasicGetOptions();

  TestGetFuture get_future;
  authenticator()->GetAssertion(std::move(get_assertion_request_params),
                                get_future.GetCallback());
  EXPECT_TRUE(get_future.Wait());
  EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR,
            std::get<0>(get_future.Get()));

  EXPECT_TRUE(logger->log().empty());
}

}  // namespace

}  // namespace content
