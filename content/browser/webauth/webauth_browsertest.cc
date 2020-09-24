// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/test/bind_test_util.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/browser/renderer_host/render_frame_host_impl.h"
#include "content/browser/webauth/authenticator_environment_impl.h"
#include "content/browser/webauth/authenticator_impl.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/navigation_throttle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/back_forward_cache_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/public/test/content_browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "content/shell/browser/shell.h"
#include "content/test/did_commit_navigation_interceptor.h"
#include "device/base/features.h"
#include "device/fido/fake_fido_discovery.h"
#include "device/fido/features.h"
#include "device/fido/fido_discovery_factory.h"
#include "device/fido/fido_test_data.h"
#include "device/fido/hid/fake_hid_impl_for_testing.h"
#include "device/fido/mock_fido_device.h"
#include "device/fido/test_callback_receiver.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webauthn/authenticator.mojom.h"

#if defined(OS_WIN)
#include <windows.h>

#include "device/fido/win/fake_webauthn_api.h"
#endif

namespace content {

namespace {

using blink::mojom::Authenticator;
using blink::mojom::AuthenticatorStatus;
using blink::mojom::GetAssertionAuthenticatorResponsePtr;
using blink::mojom::MakeCredentialAuthenticatorResponsePtr;

using TestCreateCallbackReceiver =
    ::device::test::StatusAndValueCallbackReceiver<
        AuthenticatorStatus,
        MakeCredentialAuthenticatorResponsePtr>;

using TestGetCallbackReceiver = ::device::test::StatusAndValueCallbackReceiver<
    AuthenticatorStatus,
    GetAssertionAuthenticatorResponsePtr>;

constexpr char kOkMessage[] = "webauth: OK";

constexpr char kPublicKeyErrorMessage[] =
    "webauth: NotSupportedError: Required parameters missing in "
    "`options.publicKey`.";

constexpr char kNotAllowedErrorMessage[] =
    "webauth: NotAllowedError: The operation either timed out or was not "
    "allowed. See: "
    "https://www.w3.org/TR/webauthn-2/#sctn-privacy-considerations-client.";

#if defined(OS_WIN)
constexpr char kInvalidStateErrorMessage[] =
    "webauth: InvalidStateError: The user attempted to register an "
    "authenticator that contains one of the credentials already registered "
    "with the relying party.";
#endif  // defined(OS_WIN)

constexpr char kResidentCredentialsErrorMessage[] =
    "webauth: NotSupportedError: Resident credentials or empty "
    "'allowCredentials' lists are not supported at this time.";

constexpr char kRelyingPartySecurityErrorMessage[] =
    "webauth: SecurityError: The relying party ID is not a registrable domain "
    "suffix of, nor equal to the current domain.";

constexpr char kRelyingPartyUserIconUrlSecurityErrorMessage[] =
    "webauth: SecurityError: 'user.icon' should be a secure URL";

constexpr char kRelyingPartyRpIconUrlSecurityErrorMessage[] =
    "webauth: SecurityError: 'rp.icon' should be a secure URL";

constexpr char kAbortErrorMessage[] =
    "webauth: AbortError: Request has been aborted.";

constexpr char kGetFeaturePolicyMissingMessage[] =
    "webauth: NotAllowedError: The 'publickey-credentials-get' feature is "
    "not enabled in this document. Feature Policy may be used to delegate Web "
    "Authentication capabilities to cross-origin child frames.";

constexpr char kCrossOriginAncestorMessage[] =
    "webauth: NotAllowedError: The following credential operations can only "
    "occur in a document which is same-origin with all of its ancestors: "
    "storage/retrieval of 'PasswordCredential' and 'FederatedCredential', "
    "storage of 'PublicKeyCredential'.";

// Templates to be used with base::ReplaceStringPlaceholders. Can be
// modified to include up to 9 replacements. The default values for
// any additional replacements added should also be added to the
// CreateParameters struct.
constexpr char kCreatePublicKeyTemplate[] =
    "navigator.credentials.create({ publicKey: {"
    "  challenge: new TextEncoder().encode('climb a mountain'),"
    "  rp: { id: '$3', name: 'Acme', icon: '$7'},"
    "  user: { "
    "    id: new TextEncoder().encode('1098237235409872'),"
    "    name: 'avery.a.jones@example.com',"
    "    displayName: 'Avery A. Jones', "
    "    icon: '$8'},"
    "  pubKeyCredParams: [{ type: 'public-key', alg: '$4'}],"
    "  timeout: _timeout_,"
    "  excludeCredentials: [],"
    "  authenticatorSelection: {"
    "     requireResidentKey: $1,"
    "     userVerification: '$2',"
    "     authenticatorAttachment: '$5',"
    "  },"
    "  attestation: '$6',"
    "}}).then(c => window.domAutomationController.send('webauth: OK' + $9),"
    "         e => window.domAutomationController.send("
    "                  'webauth: ' + e.toString()));";

constexpr char kCreatePublicKeyWithAbortSignalTemplate[] =
    "navigator.credentials.create({ publicKey: {"
    "  challenge: new TextEncoder().encode('climb a mountain'),"
    "  rp: { id: '$3', name: 'Acme', icon: '$7'},"
    "  user: { "
    "    id: new TextEncoder().encode('1098237235409872'),"
    "    name: 'avery.a.jones@example.com',"
    "    displayName: 'Avery A. Jones', "
    "    icon: '$8'},"
    "  pubKeyCredParams: [{ type: 'public-key', alg: '$4'}],"
    "  timeout: _timeout_,"
    "  excludeCredentials: [],"
    "  authenticatorSelection: {"
    "     requireResidentKey: $1,"
    "     userVerification: '$2',"
    "     authenticatorAttachment: '$5',"
    "  },"
    "  attestation: '$6',"
    "}, signal: $9}"
    ").then(c => window.domAutomationController.send('webauth: OK'),"
    "       e => window.domAutomationController.send("
    "                'webauth: ' + e.toString()));";

constexpr char kPlatform[] = "platform";
constexpr char kCrossPlatform[] = "cross-platform";
constexpr char kPreferredVerification[] = "preferred";
constexpr char kRequiredVerification[] = "required";
constexpr char kShortTimeout[] = "100";

// Default values for kCreatePublicKeyTemplate.
struct CreateParameters {
  const char* rp_id = "acme.com";
  bool require_resident_key = false;
  const char* user_verification = kPreferredVerification;
  const char* authenticator_attachment = kCrossPlatform;
  const char* algorithm_identifier = "-7";
  const char* attestation = "none";
  const char* rp_icon = "https://pics.acme.com/00/p/aBjjjpqPb.png";
  const char* user_icon = "https://pics.acme.com/00/p/aBjjjpqPb.png";
  const char* signal = "";
  // extra_ok_output is a Javascript expression which must evaluate to a string.
  // It can use the |PublicKeyCredential| object named |c| to extract useful
  // fields.
  const char* extra_ok_output = "''";
  const char* timeout = "1000";
};

std::string BuildCreateCallWithParameters(const CreateParameters& parameters) {
  std::vector<std::string> substitutions;
  substitutions.push_back(parameters.require_resident_key ? "true" : "false");
  substitutions.push_back(parameters.user_verification);
  substitutions.push_back(parameters.rp_id);
  substitutions.push_back(parameters.algorithm_identifier);
  substitutions.push_back(parameters.authenticator_attachment);
  substitutions.push_back(parameters.attestation);
  substitutions.push_back(parameters.rp_icon);
  substitutions.push_back(parameters.user_icon);

  std::string result;
  if (strlen(parameters.signal) == 0) {
    substitutions.push_back(parameters.extra_ok_output);
    result = base::ReplaceStringPlaceholders(kCreatePublicKeyTemplate,
                                             substitutions, nullptr);
  } else {
    substitutions.push_back(parameters.signal);
    result = base::ReplaceStringPlaceholders(
        kCreatePublicKeyWithAbortSignalTemplate, substitutions, nullptr);
  }

  base::ReplaceFirstSubstringAfterOffset(&result, 0, "_timeout_",
                                         parameters.timeout);
  return result;
}

constexpr char kGetPublicKeyTemplate[] =
    "navigator.credentials.get({ publicKey: {"
    "  challenge: new TextEncoder().encode('climb a mountain'),"
    "  timeout: $4,"
    "  userVerification: '$1',"
    "  $2}"
    "}).then(c => window.domAutomationController.send('webauth: OK' + $3),"
    "        e => window.domAutomationController.send("
    "                  'webauth: ' + e.toString()));";

constexpr char kGetPublicKeyWithAbortSignalTemplate[] =
    "navigator.credentials.get({ publicKey: {"
    "  challenge: new TextEncoder().encode('climb a mountain'),"
    "  timeout: $4,"
    "  userVerification: '$1',"
    "  $2},"
    "  signal: $5"
    "}).catch(c => window.domAutomationController.send("
    "                  'webauth: ' + c.toString()));";

// Default values for kGetPublicKeyTemplate.
struct GetParameters {
  const char* user_verification = kPreferredVerification;
  const char* allow_credentials =
      "allowCredentials: [{ type: 'public-key',"
      "     id: new TextEncoder().encode('allowedCredential'),"
      "     transports: ['usb', 'nfc', 'ble']}]";
  const char* signal = "";
  const char* timeout = "1000";
  // extra_ok_output is a Javascript expression which must evaluate to a string.
  // It can use the |PublicKeyCredential| object named |c| to extract useful
  // fields.
  const char* extra_ok_output = "''";
};

std::string BuildGetCallWithParameters(const GetParameters& parameters) {
  std::vector<std::string> substitutions;
  substitutions.push_back(parameters.user_verification);
  substitutions.push_back(parameters.allow_credentials);
  substitutions.push_back(parameters.extra_ok_output);
  substitutions.push_back(parameters.timeout);
  if (strlen(parameters.signal) == 0) {
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
  ~ClosureExecutorBeforeNavigationCommit() override = default;

 protected:
  bool WillProcessDidCommitNavigation(
      RenderFrameHost* render_frame_host,
      NavigationRequest* navigation_request,
      ::FrameHostMsg_DidCommitProvisionalLoad_Params* params,
      mojom::DidCommitProvisionalLoadInterfaceParamsPtr* interface_params)
      override {
    if (closure_)
      std::move(closure_).Run();
    return true;
  }

 private:
  base::OnceClosure closure_;
  DISALLOW_COPY_AND_ASSIGN(ClosureExecutorBeforeNavigationCommit);
};

// Cancels all navigations in a WebContents while in scope.
class ScopedNavigationCancellingThrottleInstaller : public WebContentsObserver {
 public:
  explicit ScopedNavigationCancellingThrottleInstaller(
      WebContents* web_contents)
      : WebContentsObserver(web_contents) {}
  ~ScopedNavigationCancellingThrottleInstaller() override = default;

 protected:
  class CancellingThrottle : public NavigationThrottle {
   public:
    explicit CancellingThrottle(NavigationHandle* handle)
        : NavigationThrottle(handle) {}
    ~CancellingThrottle() override = default;

   protected:
    const char* GetNameForLogging() override {
      return "ScopedNavigationCancellingThrottleInstaller::CancellingThrottle";
    }

    ThrottleCheckResult WillStartRequest() override {
      return ThrottleCheckResult(CANCEL);
    }

   private:
    DISALLOW_COPY_AND_ASSIGN(CancellingThrottle);
  };

  void DidStartNavigation(NavigationHandle* navigation_handle) override {
    navigation_handle->RegisterThrottleForTesting(
        std::make_unique<CancellingThrottle>(navigation_handle));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedNavigationCancellingThrottleInstaller);
};

struct WebAuthBrowserTestState {
  // Called when the browser is asked to display an attestation prompt. There is
  // no default so if no callback is installed then the test will crash.
  base::OnceCallback<void(base::OnceCallback<void(bool)>)>
      attestation_prompt_callback_;

  // Set when |IsFocused| is called.
  bool focus_checked = false;

  // This is incremented when an |AuthenticatorRequestClientDelegate| is
  // created.
  int delegate_create_count = 0;
};

class WebAuthBrowserTestClientDelegate
    : public AuthenticatorRequestClientDelegate {
 public:
  explicit WebAuthBrowserTestClientDelegate(WebAuthBrowserTestState* test_state)
      : test_state_(test_state) {}

  void ShouldReturnAttestation(
      const std::string& relying_party_id,
      const ::device::FidoAuthenticator* authenticator,
      bool is_enterprise_attestation,
      base::OnceCallback<void(bool)> callback) override {
    std::move(test_state_->attestation_prompt_callback_)
        .Run(std::move(callback));
  }

  bool IsFocused() override {
    test_state_->focus_checked = true;
    return AuthenticatorRequestClientDelegate::IsFocused();
  }

 private:
  WebAuthBrowserTestState* const test_state_;

  DISALLOW_COPY_AND_ASSIGN(WebAuthBrowserTestClientDelegate);
};

// Implements ContentBrowserClient and allows webauthn-related calls to be
// mocked.
class WebAuthBrowserTestContentBrowserClient : public ContentBrowserClient {
 public:
  explicit WebAuthBrowserTestContentBrowserClient(
      WebAuthBrowserTestState* test_state)
      : test_state_(test_state) {}

  std::unique_ptr<AuthenticatorRequestClientDelegate>
  GetWebAuthenticationRequestDelegate(
      RenderFrameHost* render_frame_host) override {
    test_state_->delegate_create_count++;
    return std::make_unique<WebAuthBrowserTestClientDelegate>(test_state_);
  }

 private:
  WebAuthBrowserTestState* const test_state_;

  DISALLOW_COPY_AND_ASSIGN(WebAuthBrowserTestContentBrowserClient);
};

// Test fixture base class for common tasks.
class WebAuthBrowserTestBase : public content::ContentBrowserTest {
 protected:
  WebAuthBrowserTestBase()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  virtual std::vector<base::Feature> GetFeaturesToEnable() { return {}; }

  void SetUpOnMainThread() override {
    ContentBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");
    https_server().ServeFilesFromSourceDirectory(GetTestDataFilePath());
    ASSERT_TRUE(https_server().Start());

    test_client_.reset(
        new WebAuthBrowserTestContentBrowserClient(&test_state_));
    old_client_ = SetBrowserClientForTesting(test_client_.get());

    EXPECT_TRUE(
        NavigateToURL(shell(), GetHttpsURL("www.acme.com", "/title1.html")));
  }

  void TearDown() override {
    CHECK_EQ(SetBrowserClientForTesting(old_client_), test_client_.get());
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
    AuthenticatorEnvironmentImpl::GetInstance()
        ->ReplaceDefaultDiscoveryFactoryForTesting(
            std::move(owned_virtual_device_factory));
    return virtual_device_factory;
  }

  net::EmbeddedTestServer& https_server() { return https_server_; }

  WebAuthBrowserTestState* test_state() { return &test_state_; }

 private:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    scoped_feature_list_.InitWithFeatures(GetFeaturesToEnable(), {});
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  net::EmbeddedTestServer https_server_;
  std::unique_ptr<WebAuthBrowserTestContentBrowserClient> test_client_;
  WebAuthBrowserTestState test_state_;
  ContentBrowserClient* old_client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WebAuthBrowserTestBase);
};

// WebAuthLocalClientBrowserTest ----------------------------------------------

// Browser test fixture where the blink::mojom::Authenticator interface is
// accessed from a testing client in the browser process.
class WebAuthLocalClientBrowserTest : public WebAuthBrowserTestBase {
 public:
  WebAuthLocalClientBrowserTest() {
    auto discovery_factory =
        std::make_unique<device::test::FakeFidoDiscoveryFactory>();
    discovery_factory_ = discovery_factory.get();
    AuthenticatorEnvironmentImpl::GetInstance()
        ->ReplaceDefaultDiscoveryFactoryForTesting(
            std::move(discovery_factory));
  }
  ~WebAuthLocalClientBrowserTest() override = default;

 protected:
  void SetUpOnMainThread() override {
    WebAuthBrowserTestBase::SetUpOnMainThread();
    ConnectToAuthenticator();
  }

  void ConnectToAuthenticator() {
    auto* render_frame_host_impl = static_cast<RenderFrameHostImpl*>(
        shell()->web_contents()->GetMainFrame());
    if (authenticator_remote_.is_bound())
      authenticator_remote_.reset();
    render_frame_host_impl->GetAuthenticator(
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
    auto mojo_options = blink::mojom::PublicKeyCredentialCreationOptions::New(
        rp, user, kTestChallenge, parameters, base::TimeDelta::FromSeconds(30),
        std::vector<device::PublicKeyCredentialDescriptor>(),
        device::AuthenticatorSelectionCriteria(),
        device::AttestationConveyancePreference::kNone,
        /*cable_registration_data=*/nullptr,
        /*hmac_create_secret=*/false, /*prf_enable=*/false,
        blink::mojom::ProtectionPolicy::UNSPECIFIED,
        /*enforce_protection_policy=*/false, /*appid_exclude=*/base::nullopt,
        /*cred_props=*/false);

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
    auto mojo_options = blink::mojom::PublicKeyCredentialRequestOptions::New(
        kTestChallenge, base::TimeDelta::FromSeconds(30), "acme.com",
        std::move(credentials), device::UserVerificationRequirement::kPreferred,
        base::nullopt, std::vector<device::CableDiscoveryData>(), /*prf=*/false,
        /*prf_inputs=*/std::vector<blink::mojom::PRFValuesPtr>());
    return mojo_options;
  }

  void WaitForConnectionError() {
    ASSERT_TRUE(authenticator_remote_);
    ASSERT_TRUE(authenticator_remote_.is_bound());
    if (!authenticator_remote_.is_connected())
      return;

    base::RunLoop run_loop;
    authenticator_remote_.set_disconnect_handler(run_loop.QuitClosure());
    run_loop.Run();
  }

  blink::mojom::Authenticator* authenticator() {
    return authenticator_remote_.get();
  }

  device::test::FakeFidoDiscoveryFactory* discovery_factory_;

 private:
  mojo::Remote<blink::mojom::Authenticator> authenticator_remote_;

  DISALLOW_COPY_AND_ASSIGN(WebAuthLocalClientBrowserTest);
};

// Tests that no crash occurs when the implementation is destroyed with a
// pending navigator.credentials.create({publicKey: ...}) call.
IN_PROC_BROWSER_TEST_F(WebAuthLocalClientBrowserTest,
                       CreatePublicKeyCredentialThenNavigateAway) {
  auto* fake_hid_discovery = discovery_factory_->ForgeNextHidDiscovery();
  TestCreateCallbackReceiver create_callback_receiver;
  authenticator()->MakeCredential(BuildBasicCreateOptions(),
                                  create_callback_receiver.callback());

  fake_hid_discovery->WaitForCallToStartAndSimulateSuccess();
  EXPECT_TRUE(
      NavigateToURL(shell(), GetHttpsURL("www.acme.com", "/title2.html")));
  WaitForConnectionError();

  // The next active document should be able to successfully call
  // navigator.credentials.create({publicKey: ...}) again.
  ConnectToAuthenticator();
  fake_hid_discovery = discovery_factory_->ForgeNextHidDiscovery();
  authenticator()->MakeCredential(BuildBasicCreateOptions(),
                                  create_callback_receiver.callback());
  fake_hid_discovery->WaitForCallToStartAndSimulateSuccess();
}

// Tests that no crash occurs when the implementation is destroyed with a
// pending navigator.credentials.get({publicKey: ...}) call.
IN_PROC_BROWSER_TEST_F(WebAuthLocalClientBrowserTest,
                       GetPublicKeyCredentialThenNavigateAway) {
  auto* fake_hid_discovery = discovery_factory_->ForgeNextHidDiscovery();
  TestGetCallbackReceiver get_callback_receiver;
  authenticator()->GetAssertion(BuildBasicGetOptions(),
                                get_callback_receiver.callback());

  fake_hid_discovery->WaitForCallToStartAndSimulateSuccess();
  EXPECT_TRUE(
      NavigateToURL(shell(), GetHttpsURL("www.acme.com", "/title2.html")));
  WaitForConnectionError();

  // The next active document should be able to successfully call
  // navigator.credentials.get({publicKey: ...}) again.
  ConnectToAuthenticator();
  fake_hid_discovery = discovery_factory_->ForgeNextHidDiscovery();
  authenticator()->GetAssertion(BuildBasicGetOptions(),
                                get_callback_receiver.callback());
  fake_hid_discovery->WaitForCallToStartAndSimulateSuccess();
}

enum class AttestationCallbackBehavior {
  IGNORE_CALLBACK,
  BEFORE_NAVIGATION,
  AFTER_NAVIGATION,
};

const char* AttestationCallbackBehaviorToString(
    AttestationCallbackBehavior behavior) {
  switch (behavior) {
    case AttestationCallbackBehavior::IGNORE_CALLBACK:
      return "IGNORE_CALLBACK";
    case AttestationCallbackBehavior::BEFORE_NAVIGATION:
      return "BEFORE_NAVIGATION";
    case AttestationCallbackBehavior::AFTER_NAVIGATION:
      return "AFTER_NAVIGATION";
  }
}

const AttestationCallbackBehavior kAllAttestationCallbackBehaviors[] = {
    AttestationCallbackBehavior::IGNORE_CALLBACK,
    AttestationCallbackBehavior::BEFORE_NAVIGATION,
    AttestationCallbackBehavior::AFTER_NAVIGATION,
};

// Tests navigating while an attestation permission prompt is showing.
IN_PROC_BROWSER_TEST_F(WebAuthLocalClientBrowserTest,
                       PromptForAttestationThenNavigateAway) {
  for (auto behavior : kAllAttestationCallbackBehaviors) {
    SCOPED_TRACE(AttestationCallbackBehaviorToString(behavior));

    InjectVirtualFidoDeviceFactory();
    TestCreateCallbackReceiver create_callback_receiver;
    auto options = BuildBasicCreateOptions();
    options->attestation = device::AttestationConveyancePreference::kDirect;
    authenticator()->MakeCredential(std::move(options),
                                    create_callback_receiver.callback());
    bool attestation_callback_was_invoked = false;
    test_state()->attestation_prompt_callback_ = base::BindLambdaForTesting(
        [&](base::OnceCallback<void(bool)> callback) {
          attestation_callback_was_invoked = true;

          if (behavior == AttestationCallbackBehavior::BEFORE_NAVIGATION) {
            std::move(callback).Run(false);
          }
          EXPECT_TRUE(NavigateToURL(
              shell(), GetHttpsURL("www.acme.com", "/title2.html")));
          if (behavior == AttestationCallbackBehavior::AFTER_NAVIGATION) {
            std::move(callback).Run(false);
          }
        });

    WaitForConnectionError();
    ASSERT_TRUE(attestation_callback_was_invoked);
    ConnectToAuthenticator();
  }
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

  auto* fake_hid_discovery = discovery_factory_->ForgeNextHidDiscovery();
  TestCreateCallbackReceiver create_callback_receiver;
  authenticator()->MakeCredential(BuildBasicCreateOptions(),
                                  create_callback_receiver.callback());

  fake_hid_discovery->WaitForCallToStartAndSimulateSuccess();
}

// Tests that a navigator.credentials.create({publicKey: ...}) issued at the
// moment just before a navigation commits is not serviced.
IN_PROC_BROWSER_TEST_F(WebAuthLocalClientBrowserTest,
                       CreatePublicKeyCredentialRacingWithNavigation) {
  TestCreateCallbackReceiver create_callback_receiver;
  auto request_options = BuildBasicCreateOptions();

  ClosureExecutorBeforeNavigationCommit executor(
      shell()->web_contents(), base::BindLambdaForTesting([&]() {
        authenticator()->MakeCredential(std::move(request_options),
                                        create_callback_receiver.callback());
      }));

  auto* fake_hid_discovery = discovery_factory_->ForgeNextHidDiscovery();
  EXPECT_TRUE(
      NavigateToURL(shell(), GetHttpsURL("www.acme.com", "/title2.html")));
  WaitForConnectionError();

  // Normally, when the request is serviced, the implementation retrieves the
  // factory as one of the first steps. Here, the request should not have been
  // serviced at all, so the fake request should still be pending on the fake
  // factory.
  std::vector<std::unique_ptr<device::FidoDiscoveryBase>> discoveries =
      discovery_factory_->Create(
          ::device::FidoTransportProtocol::kUsbHumanInterfaceDevice);
  EXPECT_EQ(discoveries.size(), 1u);

  // The next active document should be able to successfully call
  // navigator.credentials.create({publicKey: ...}) again.
  ConnectToAuthenticator();
  fake_hid_discovery = discovery_factory_->ForgeNextHidDiscovery();
  authenticator()->MakeCredential(BuildBasicCreateOptions(),
                                  create_callback_receiver.callback());
  fake_hid_discovery->WaitForCallToStartAndSimulateSuccess();
}

// Regression test for https://crbug.com/818219.
IN_PROC_BROWSER_TEST_F(WebAuthLocalClientBrowserTest,
                       CreatePublicKeyCredentialTwiceInARow) {
  TestCreateCallbackReceiver callback_receiver_1;
  TestCreateCallbackReceiver callback_receiver_2;
  authenticator()->MakeCredential(BuildBasicCreateOptions(),
                                  callback_receiver_1.callback());
  authenticator()->MakeCredential(BuildBasicCreateOptions(),
                                  callback_receiver_2.callback());
  callback_receiver_2.WaitForCallback();

  EXPECT_EQ(AuthenticatorStatus::PENDING_REQUEST, callback_receiver_2.status());
  EXPECT_FALSE(callback_receiver_1.was_called());
}

// Regression test for https://crbug.com/818219.
IN_PROC_BROWSER_TEST_F(WebAuthLocalClientBrowserTest,
                       GetPublicKeyCredentialTwiceInARow) {
  TestGetCallbackReceiver callback_receiver_1;
  TestGetCallbackReceiver callback_receiver_2;
  authenticator()->GetAssertion(BuildBasicGetOptions(),
                                callback_receiver_1.callback());
  authenticator()->GetAssertion(BuildBasicGetOptions(),
                                callback_receiver_2.callback());
  callback_receiver_2.WaitForCallback();

  EXPECT_EQ(AuthenticatorStatus::PENDING_REQUEST, callback_receiver_2.status());
  EXPECT_FALSE(callback_receiver_1.was_called());
}

IN_PROC_BROWSER_TEST_F(WebAuthLocalClientBrowserTest,
                       CreatePublicKeyCredentialWhileRequestIsPending) {
  auto* fake_hid_discovery = discovery_factory_->ForgeNextHidDiscovery();
  TestCreateCallbackReceiver callback_receiver_1;
  TestCreateCallbackReceiver callback_receiver_2;
  authenticator()->MakeCredential(BuildBasicCreateOptions(),
                                  callback_receiver_1.callback());
  fake_hid_discovery->WaitForCallToStartAndSimulateSuccess();

  authenticator()->MakeCredential(BuildBasicCreateOptions(),
                                  callback_receiver_2.callback());
  callback_receiver_2.WaitForCallback();

  EXPECT_EQ(AuthenticatorStatus::PENDING_REQUEST, callback_receiver_2.status());
  EXPECT_FALSE(callback_receiver_1.was_called());
}

IN_PROC_BROWSER_TEST_F(WebAuthLocalClientBrowserTest,
                       GetPublicKeyCredentialWhileRequestIsPending) {
  auto* fake_hid_discovery = discovery_factory_->ForgeNextHidDiscovery();
  TestGetCallbackReceiver callback_receiver_1;
  TestGetCallbackReceiver callback_receiver_2;
  authenticator()->GetAssertion(BuildBasicGetOptions(),
                                callback_receiver_1.callback());
  fake_hid_discovery->WaitForCallToStartAndSimulateSuccess();

  authenticator()->GetAssertion(BuildBasicGetOptions(),
                                callback_receiver_2.callback());
  callback_receiver_2.WaitForCallback();

  EXPECT_EQ(AuthenticatorStatus::PENDING_REQUEST, callback_receiver_2.status());
  EXPECT_FALSE(callback_receiver_1.was_called());
}

// WebAuthJavascriptClientBrowserTest -----------------------------------------

// Browser test fixture where the blink::mojom::Authenticator interface is
// normally accessed from Javascript in the renderer process.
class WebAuthJavascriptClientBrowserTest : public WebAuthBrowserTestBase {
 public:
  WebAuthJavascriptClientBrowserTest() = default;
  ~WebAuthJavascriptClientBrowserTest() override = default;

 protected:
  std::vector<base::Feature> GetFeaturesToEnable() override {
    return {device::kWebAuthGetAssertionFeaturePolicy};
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(WebAuthJavascriptClientBrowserTest);
};

constexpr device::ProtocolVersion kAllProtocols[] = {
    device::ProtocolVersion::kCtap2, device::ProtocolVersion::kU2f};

// Tests that when navigator.credentials.create() is called with an invalid
// relying party id, we get a SecurityError.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       CreatePublicKeyCredentialInvalidRp) {
  CreateParameters parameters;
  parameters.rp_id = "localhost";
  std::string result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      shell()->web_contents()->GetMainFrame(),
      BuildCreateCallWithParameters(parameters), &result));

  ASSERT_EQ(kRelyingPartySecurityErrorMessage,
            result.substr(0, strlen(kRelyingPartySecurityErrorMessage)));
}

// Tests that when navigator.credentials.create() is called with a null
// relying party, we get a NotSupportedError.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       CreatePublicKeyWithNullRp) {
  CreateParameters parameters;
  parameters.rp_icon = "";
  std::string script = BuildCreateCallWithParameters(parameters);
  const char kExpectedSubstr[] = "{ id: 'acme.com', name: 'Acme', icon: ''}";
  const std::string::size_type offset = script.find(kExpectedSubstr);
  ASSERT_TRUE(offset != std::string::npos);
  script.replace(offset, sizeof(kExpectedSubstr) - 1, "null");

  std::string result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      shell()->web_contents()->GetMainFrame(), script, &result));
  ASSERT_EQ(kPublicKeyErrorMessage, result);
}

// Tests that when navigator.credentials.create() is called with an insecure
// user icon URL, we get a SecurityError.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       CreatePublicKeyWithInsecureUserIconURL) {
  CreateParameters parameters;
  parameters.user_icon = "http://fidoalliance.co.nz/testimages/catimage.png";
  std::string result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      shell()->web_contents()->GetMainFrame(),
      BuildCreateCallWithParameters(parameters), &result));
  ASSERT_EQ(kRelyingPartyUserIconUrlSecurityErrorMessage, result);
}

// Tests that when navigator.credentials.create() is called with an insecure
// Relying Party icon URL, we get a SecurityError.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       CreatePublicKeyWithInsecureRpIconURL) {
  CreateParameters parameters;
  parameters.rp_icon = "http://fidoalliance.co.nz/testimages/catimage.png";
  std::string result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      shell()->web_contents()->GetMainFrame(),
      BuildCreateCallWithParameters(parameters), &result));
  ASSERT_EQ(kRelyingPartyRpIconUrlSecurityErrorMessage, result);
}

// Tests that when navigator.credentials.create() is called with user
// verification required, request times out.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       CreatePublicKeyCredentialWithUserVerification) {
  for (const auto protocol : kAllProtocols) {
    auto* virtual_device_factory = InjectVirtualFidoDeviceFactory();
    virtual_device_factory->SetSupportedProtocol(protocol);

    CreateParameters parameters;
    parameters.user_verification = kRequiredVerification;
    parameters.timeout = kShortTimeout;
    std::string result;
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        shell()->web_contents()->GetMainFrame(),
        BuildCreateCallWithParameters(parameters), &result));
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
    std::string result;
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        shell()->web_contents()->GetMainFrame(),
        BuildCreateCallWithParameters(parameters), &result));

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
    std::string result;
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        shell()->web_contents()->GetMainFrame(),
        BuildCreateCallWithParameters(parameters), &result));

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
    parameters.authenticator_attachment = kPlatform;
    parameters.timeout = kShortTimeout;
    std::string result;
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        shell()->web_contents()->GetMainFrame(),
        BuildCreateCallWithParameters(parameters), &result));

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
    std::string result;
    std::string script =
        "authAbortController = new AbortController();"
        "authAbortSignal = authAbortController.signal;" +
        BuildCreateCallWithParameters(parameters);

    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        shell()->web_contents()->GetMainFrame(), script, &result));
    ASSERT_EQ(kOkMessage, result);
  }
}

// Tests that when navigator.credentials.create() is called with abort
// signal's aborted flag set before sending request, we get an AbortError.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       CreatePublicKeyCredentialWithAbortSetBeforeCreate) {
  CreateParameters parameters;
  parameters.signal = "authAbortSignal";
  std::string result;
  std::string script =
      "authAbortController = new AbortController();"
      "authAbortSignal = authAbortController.signal;"
      "authAbortController.abort();" +
      BuildCreateCallWithParameters(parameters);

  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      shell()->web_contents()->GetMainFrame(), script, &result));
  ASSERT_EQ(kAbortErrorMessage, result.substr(0, strlen(kAbortErrorMessage)));
}

// Tests that when navigator.credentials.create() is called with abort
// signal's aborted flag set after sending request, we get an AbortError.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       CreatePublicKeyCredentialWithAbortSetAfterCreate) {
  InjectVirtualFidoDeviceFactory();
  CreateParameters parameters;
  parameters.signal = "authAbortSignal";
  std::string result;
  std::string script =
      "authAbortController = new AbortController();"
      "authAbortSignal = authAbortController.signal;" +
      BuildCreateCallWithParameters(parameters) +
      "authAbortController.abort();";

  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      shell()->web_contents()->GetMainFrame(), script, &result));
  ASSERT_EQ(kAbortErrorMessage, result.substr(0, strlen(kAbortErrorMessage)));
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
    std::string result;
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        shell()->web_contents()->GetMainFrame(),
        BuildGetCallWithParameters(parameters), &result));
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
      "allowCredentials: [{"
      "  type: 'public-key',"
      "  id: new TextEncoder().encode('allowedCredential'),"
      "  transports: ['carrierpigeon'],"
      "}]";
  parameters.timeout = kShortTimeout;
  std::string result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      shell()->web_contents()->GetMainFrame(),
      BuildGetCallWithParameters(parameters), &result));
  ASSERT_EQ(kNotAllowedErrorMessage, result);
}

// Tests that when navigator.credentials.get() is called with an empty
// allowCredentials list, we get a NotSupportedError.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       GetPublicKeyCredentialEmptyAllowCredentialsList) {
  InjectVirtualFidoDeviceFactory();
  GetParameters parameters;
  parameters.allow_credentials = "";
  std::string result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      shell()->web_contents()->GetMainFrame(),
      BuildGetCallWithParameters(parameters), &result));
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
    std::string result;
    std::string script =
        "authAbortController = new AbortController();"
        "authAbortSignal = authAbortController.signal;" +
        BuildGetCallWithParameters(parameters);

    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        shell()->web_contents()->GetMainFrame(), script, &result));
    ASSERT_EQ(kNotAllowedErrorMessage, result);
  }
}

// Tests that when navigator.credentials.get() is called with abort
// signal's aborted flag set before sending request, we get an AbortError.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       GetPublicKeyCredentialWithAbortSetBeforeGet) {
  GetParameters parameters;
  parameters.signal = "authAbortSignal";
  std::string result;
  std::string script =
      "authAbortController = new AbortController();"
      "authAbortSignal = authAbortController.signal;"
      "authAbortController.abort();" +
      BuildGetCallWithParameters(parameters);

  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      shell()->web_contents()->GetMainFrame(), script, &result));
  ASSERT_EQ(kAbortErrorMessage, result.substr(0, strlen(kAbortErrorMessage)));
}

// Tests that when navigator.credentials.get() is called with abort
// signal's aborted flag set after sending request, we get an AbortError.
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       GetPublicKeyCredentialWithAbortSetAfterGet) {
  InjectVirtualFidoDeviceFactory();
  GetParameters parameters;
  parameters.signal = "authAbortSignal";
  std::string result;
  std::string script =
      "authAbortController = new AbortController();"
      "authAbortSignal = authAbortController.signal;" +
      BuildGetCallWithParameters(parameters) + "authAbortController.abort();";

  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      shell()->web_contents()->GetMainFrame(), script, &result));
  ASSERT_EQ(kAbortErrorMessage, result.substr(0, strlen(kAbortErrorMessage)));
}

// Executes Javascript in the given WebContents and waits until a string with
// the given prefix is received. It will ignore values other than strings, and
// strings without the given prefix. Since messages are broadcast to
// DOMMessageQueues, this allows other functions that depend on ExecuteScript
// (and thus trigger the broadcast of values) to run while this function is
// waiting for a specific result.
base::Optional<std::string> ExecuteScriptAndExtractPrefixedString(
    WebContents* web_contents,
    const std::string& script,
    const std::string& result_prefix) {
  DOMMessageQueue dom_message_queue(web_contents);
  web_contents->GetMainFrame()->ExecuteJavaScriptForTests(
      base::UTF8ToUTF16(script), base::NullCallback());

  for (;;) {
    std::string json;
    if (!dom_message_queue.WaitForMessage(&json)) {
      return base::nullopt;
    }

    base::Optional<base::Value> result =
        base::JSONReader::Read(json, base::JSON_ALLOW_TRAILING_COMMAS);
    if (!result) {
      return base::nullopt;
    }

    std::string str;
    if (result->GetAsString(&str) && str.find(result_prefix) == 0) {
      return str;
    }
  }
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
  };

  for (const auto& test : kTestCases) {
    SCOPED_TRACE(test.allow_value);
    SCOPED_TRACE(test.cross_origin);

    const std::string setAllowJS = base::StringPrintf(
        "document.getElementById('test_iframe').setAttribute('allow', '%s'); "
        "window.domAutomationController.send('OK');",
        test.allow_value);
    std::string result;
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        shell()->web_contents()->GetMainFrame(), setAllowJS.c_str(), &result));
    ASSERT_EQ("OK", result);

    if (test.cross_origin) {
      // Create a cross-origin iframe by loading it from notacme.com.
      NavigateIframeToURL(shell()->web_contents(), "test_iframe",
                          GetHttpsURL(kInnerHost, "/title2.html"));
    } else {
      // Create a same-origin iframe by loading it from acme.com.
      NavigateIframeToURL(shell()->web_contents(), "test_iframe",
                          GetHttpsURL(kOuterHost, "/title2.html"));
    }

    std::vector<RenderFrameHost*> frames =
        shell()->web_contents()->GetAllFrames();
    // GetAllFrames is documented to return a breadth-first list of frames. Thus
    // there should be exactly two: the main frame and the contained iframe.
    ASSERT_EQ(2u, frames.size());
    RenderFrameHost* const iframe = frames[1];

    CreateParameters create_parameters;
    create_parameters.rp_id = test.cross_origin ? "notacme.com" : "acme.com";
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        iframe, BuildCreateCallWithParameters(create_parameters), &result));
    if (test.create_should_work) {
      EXPECT_EQ(std::string(kOkMessage), result);
    } else {
      EXPECT_EQ(kCrossOriginAncestorMessage, result);
    }

    const int credential_id =
        test.cross_origin ? kInnerCredentialID : kOuterCredentialID;
    const std::string allow_credentials = base::StringPrintf(
        "allowCredentials: "
        "[{ type: 'public-key',"
        "   id: new Uint8Array([%d]),"
        "}]",
        credential_id);
    GetParameters get_params;
    get_params.allow_credentials = allow_credentials.c_str();
    ASSERT_TRUE(content::ExecuteScriptAndExtractString(
        iframe, BuildGetCallWithParameters(get_params), &result));
    if (test.get_should_work) {
      EXPECT_EQ(std::string(kOkMessage), result);
    } else {
      EXPECT_EQ(kGetFeaturePolicyMissingMessage, result);
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

  // The plain ExecuteScriptAndExtractString cannot be used because
  // NavigateIframeToURL uses it internally and they get confused about which
  // message is for whom.
  base::Optional<std::string> result = ExecuteScriptAndExtractPrefixedString(
      shell()->web_contents(),
      BuildCreateCallWithParameters(CreateParameters()), "webauth: ");
  ASSERT_TRUE(result);
  ASSERT_EQ(kOkMessage, *result);
  ASSERT_TRUE(prompt_callback_was_invoked);
}

IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       NavigateSubframeDuringAttestationPrompt) {
  InjectVirtualFidoDeviceFactory();

  for (auto behavior : kAllAttestationCallbackBehaviors) {
    if (behavior == AttestationCallbackBehavior::IGNORE_CALLBACK) {
      // If the callback is ignored, then the registration will not complete and
      // that hangs the test.
      continue;
    }

    SCOPED_TRACE(AttestationCallbackBehaviorToString(behavior));

    bool prompt_callback_was_invoked = false;
    test_state()->attestation_prompt_callback_ = base::BindOnce(
        [](WebContents* web_contents, bool* prompt_callback_was_invoked,
           AttestationCallbackBehavior behavior,
           base::OnceCallback<void(bool)> callback) {
          *prompt_callback_was_invoked = true;

          if (behavior == AttestationCallbackBehavior::BEFORE_NAVIGATION) {
            std::move(callback).Run(true);
          }
          // Can't use NavigateIframeToURL here because in the
          // BEFORE_NAVIGATION case we are racing AuthenticatorImpl and
          // NavigateIframeToURL can get confused by the "OK" message.
          base::Optional<std::string> result =
              ExecuteScriptAndExtractPrefixedString(
                  web_contents,
                  "document.getElementById('test_iframe').src = "
                  "'/title2.html'; "
                  "window.domAutomationController.send('iframe: done');",
                  "iframe: ");
          CHECK(result);
          CHECK_EQ("iframe: done", *result);
          if (behavior == AttestationCallbackBehavior::AFTER_NAVIGATION) {
            std::move(callback).Run(true);
          }
        },
        shell()->web_contents(), &prompt_callback_was_invoked, behavior);

    EXPECT_TRUE(NavigateToURL(
        shell(), GetHttpsURL("www.acme.com", "/page_with_iframe.html")));

    CreateParameters parameters;
    parameters.attestation = "direct";
    // The plain ExecuteScriptAndExtractString cannot be used because
    // NavigateIframeToURL uses it internally and they get confused about which
    // message is for whom.
    base::Optional<std::string> result = ExecuteScriptAndExtractPrefixedString(
        shell()->web_contents(), BuildCreateCallWithParameters(parameters),
        "webauth: ");
    ASSERT_TRUE(result);
    ASSERT_EQ(kOkMessage, *result);
    ASSERT_TRUE(prompt_callback_was_invoked);
  }
}

IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       BadCableExtensionVersions) {
  // The caBLE extension should only contain v1 data. Test that nothing crashes
  // if a site tries to set other versions.

  InjectVirtualFidoDeviceFactory();
  GetParameters parameters;
  parameters.allow_credentials =
      "allowCredentials: [{ type: 'public-key',"
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
  std::string result;
  ASSERT_TRUE(content::ExecuteScriptAndExtractString(
      shell()->web_contents()->GetMainFrame(),
      BuildGetCallWithParameters(parameters), &result));
  ASSERT_EQ(kNotAllowedErrorMessage, result);
}

#if defined(OS_WIN)
IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest, WinMakeCredential) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GetHttpsURL("www.acme.com", "/title1.html")));

  device::FakeWinWebAuthnApi fake_api;
  fake_api.set_is_uvpaa(true);
  fake_api.set_hresult(S_OK);
  auto* virtual_device_factory = InjectVirtualFidoDeviceFactory();
  virtual_device_factory->set_win_webauthn_api(&fake_api);

  base::Optional<std::string> result = ExecuteScriptAndExtractPrefixedString(
      shell()->web_contents(),
      BuildCreateCallWithParameters(CreateParameters()), "webauth: ");
  ASSERT_TRUE(result);
  ASSERT_EQ(kOkMessage, *result);
}

IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       WinMakeCredentialReturnCodeFailure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GetHttpsURL("www.acme.com", "/title1.html")));
  device::FakeWinWebAuthnApi fake_api;
  auto* virtual_device_factory = InjectVirtualFidoDeviceFactory();
  virtual_device_factory->set_win_webauthn_api(&fake_api);

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

    base::Optional<std::string> result = ExecuteScriptAndExtractPrefixedString(
        shell()->web_contents(),
        BuildCreateCallWithParameters(CreateParameters()), "webauth: ");
    EXPECT_TRUE(result);
    EXPECT_EQ(*result, error.second);
  }
}

IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest, WinGetAssertion) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GetHttpsURL("www.acme.com", "/title1.html")));

  constexpr uint8_t credential_id[] = {'A', 'A', 'A'};

  device::FakeWinWebAuthnApi fake_api;
  fake_api.InjectNonDiscoverableCredential(credential_id, "www.acme.com");
  auto* virtual_device_factory = InjectVirtualFidoDeviceFactory();
  virtual_device_factory->set_win_webauthn_api(&fake_api);

  GetParameters get_parameters;
  get_parameters.allow_credentials =
      "allowCredentials: [{ type: 'public-key', id: new "
      "TextEncoder().encode('AAA')}]";

  base::Optional<std::string> result = ExecuteScriptAndExtractPrefixedString(
      shell()->web_contents(), BuildGetCallWithParameters(get_parameters),
      "webauth: ");
  ASSERT_TRUE(result);
  ASSERT_EQ(kOkMessage, *result);
}

IN_PROC_BROWSER_TEST_F(WebAuthJavascriptClientBrowserTest,
                       WinGetAssertionReturnCodeFailure) {
  EXPECT_TRUE(
      NavigateToURL(shell(), GetHttpsURL("www.acme.com", "/title1.html")));
  device::FakeWinWebAuthnApi fake_api;
  auto* virtual_device_factory = InjectVirtualFidoDeviceFactory();
  virtual_device_factory->set_win_webauthn_api(&fake_api);

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

    base::Optional<std::string> result = ExecuteScriptAndExtractPrefixedString(
        shell()->web_contents(), BuildGetCallWithParameters(GetParameters()),
        "webauth: ");
    ASSERT_EQ(*result, kNotAllowedErrorMessage);
  }
}
#endif

class WebAuthLocalClientBackForwardCacheBrowserTest
    : public WebAuthLocalClientBrowserTest {
 protected:
  BackForwardCacheDisabledTester tester_;
};

IN_PROC_BROWSER_TEST_F(WebAuthLocalClientBackForwardCacheBrowserTest,
                       WebAuthDisablesBackForwardCache) {
  // Initialisation of the test should disable bfcache.
  EXPECT_TRUE(tester_.IsDisabledForFrameWithReason(
      shell()->web_contents()->GetMainFrame()->GetProcess()->GetID(),
      shell()->web_contents()->GetMainFrame()->GetRoutingID(),
      "WebAuthenticationAPI"));
}

// WebAuthBrowserCtapTest ----------------------------------------------

class WebAuthBrowserCtapTest : public WebAuthLocalClientBrowserTest {
 public:
  WebAuthBrowserCtapTest() = default;
  ~WebAuthBrowserCtapTest() override = default;

  DISALLOW_COPY_AND_ASSIGN(WebAuthBrowserCtapTest);
};

IN_PROC_BROWSER_TEST_F(WebAuthBrowserCtapTest, TestMakeCredential) {
  for (const auto protocol : kAllProtocols) {
    auto* virtual_device_factory = InjectVirtualFidoDeviceFactory();
    virtual_device_factory->SetSupportedProtocol(protocol);

    TestCreateCallbackReceiver create_callback_receiver;
    authenticator()->MakeCredential(BuildBasicCreateOptions(),
                                    create_callback_receiver.callback());

    create_callback_receiver.WaitForCallback();
    EXPECT_EQ(AuthenticatorStatus::SUCCESS, create_callback_receiver.status());
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

    TestCreateCallbackReceiver create_callback_receiver;
    authenticator()->MakeCredential(std::move(make_credential_request),
                                    create_callback_receiver.callback());

    create_callback_receiver.WaitForCallback();
    EXPECT_EQ(AuthenticatorStatus::CREDENTIAL_EXCLUDED,
              create_callback_receiver.status());
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

    TestGetCallbackReceiver get_callback_receiver;
    authenticator()->GetAssertion(std::move(get_assertion_request_params),
                                  get_callback_receiver.callback());
    get_callback_receiver.WaitForCallback();
    EXPECT_EQ(AuthenticatorStatus::SUCCESS, get_callback_receiver.status());
  }
}

IN_PROC_BROWSER_TEST_F(WebAuthBrowserCtapTest,
                       TestGetAssertionWithNoMatchingKeyHandles) {
  for (const auto protocol : kAllProtocols) {
    auto* virtual_device_factory = InjectVirtualFidoDeviceFactory();
    virtual_device_factory->SetSupportedProtocol(protocol);
    auto get_assertion_request_params = BuildBasicGetOptions();

    TestGetCallbackReceiver get_callback_receiver;
    authenticator()->GetAssertion(std::move(get_assertion_request_params),
                                  get_callback_receiver.callback());
    get_callback_receiver.WaitForCallback();
    EXPECT_EQ(AuthenticatorStatus::NOT_ALLOWED_ERROR,
              get_callback_receiver.status());
  }
}

}  // namespace

}  // namespace content
