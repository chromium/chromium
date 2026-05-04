// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/content/browser/internal_authenticator_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/strings/strcat.h"
#include "base/test/test_future.h"
#include "components/webauthn/core/browser/webauthn_test_utils.h"
#include "content/public/browser/authenticator_request_client_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/scoped_authenticator_environment_for_testing.h"
#include "content/public/common/content_client.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_renderer_host.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using blink::mojom::AuthenticationExtensionsClientInputs;
using blink::mojom::AuthenticatorStatus;
using blink::mojom::PublicKeyCredentialCreationOptions;
using blink::mojom::PublicKeyCredentialCreationOptionsPtr;
using blink::mojom::PublicKeyCredentialRequestOptions;
using blink::mojom::PublicKeyCredentialRequestOptionsPtr;

using TestMakeCredentialFuture =
    base::test::TestFuture<blink::mojom::AuthenticatorStatus,
                           blink::mojom::MakeCredentialAuthenticatorResponsePtr,
                           blink::mojom::WebAuthnDOMExceptionDetailsPtr>;
using TestGetAssertionFuture =
    base::test::TestFuture<blink::mojom::AuthenticatorStatus,
                           blink::mojom::GetAssertionAuthenticatorResponsePtr,
                           blink::mojom::WebAuthnDOMExceptionDetailsPtr>;

constexpr char kTestOrigin1[] = "https://a.google.com";
constexpr char kTestRelyingPartyId[] = "google.com";
constexpr size_t kTestCredentialIdLength = 32u;

}  // namespace

device::PublicKeyCredentialUserEntity GetTestPublicKeyCredentialUserEntity() {
  device::PublicKeyCredentialUserEntity entity;
  entity.display_name = "User A. Name";
  std::vector<uint8_t> id(32, 0x0A);
  entity.id = id;
  entity.name = "username@example.com";
  return entity;
}

device::AuthenticatorSelectionCriteria GetTestAuthenticatorSelectionCriteria() {
  return device::AuthenticatorSelectionCriteria(
      device::AuthenticatorAttachment::kAny,
      device::ResidentKeyRequirement::kDiscouraged,
      device::UserVerificationRequirement::kPreferred);
}

std::vector<device::PublicKeyCredentialDescriptor> GetTestCredentials(
    size_t num_credentials = 1) {
  std::vector<device::PublicKeyCredentialDescriptor> descriptors;
  for (size_t i = 0; i < num_credentials; i++) {
    DCHECK(i <= std::numeric_limits<uint8_t>::max());
    std::vector<uint8_t> id(kTestCredentialIdLength, static_cast<uint8_t>(i));
    base::flat_set<device::FidoTransportProtocol> transports{
        device::FidoTransportProtocol::kUsbHumanInterfaceDevice,
        device::FidoTransportProtocol::kBluetoothLowEnergy};
    descriptors.emplace_back(device::CredentialType::kPublicKey, std::move(id),
                             std::move(transports));
  }
  return descriptors;
}

std::vector<device::PublicKeyCredentialParams::CredentialInfo>
GetTestPublicKeyCredentialParameters(int32_t algorithm_identifier) {
  std::vector<device::PublicKeyCredentialParams::CredentialInfo> parameters;
  device::PublicKeyCredentialParams::CredentialInfo fake_parameter;
  fake_parameter.type = device::CredentialType::kPublicKey;
  fake_parameter.algorithm = algorithm_identifier;
  parameters.push_back(std::move(fake_parameter));
  return parameters;
}

device::PublicKeyCredentialRpEntity GetTestPublicKeyCredentialRPEntity() {
  device::PublicKeyCredentialRpEntity entity;
  entity.id = std::string(kTestRelyingPartyId);
  entity.name = "TestRP@example.com";
  return entity;
}

PublicKeyCredentialCreationOptionsPtr
GetTestPublicKeyCredentialCreationOptions() {
  auto options = PublicKeyCredentialCreationOptions::New();
  options->relying_party = GetTestPublicKeyCredentialRPEntity();
  options->user = GetTestPublicKeyCredentialUserEntity();
  options->public_key_parameters = GetTestPublicKeyCredentialParameters(
      static_cast<int32_t>(device::CoseAlgorithmIdentifier::kEs256));
  options->challenge.assign(32, 0x0A);
  options->timeout = base::Minutes(1);
  options->authenticator_selection = GetTestAuthenticatorSelectionCriteria();
  return options;
}

PublicKeyCredentialRequestOptionsPtr
GetTestPublicKeyCredentialRequestOptions() {
  auto options = PublicKeyCredentialRequestOptions::New();
  options->extensions = AuthenticationExtensionsClientInputs::New();
  options->relying_party_id = std::string(kTestRelyingPartyId);
  options->challenge = std::vector<uint8_t>(32, 0x0A);
  options->timeout = base::Minutes(1);
  options->user_verification = device::UserVerificationRequirement::kPreferred;
  options->allow_credentials = GetTestCredentials();
  return options;
}

class TestAuthenticatorContentBrowserClient : public ContentBrowserClient {
 public:
  bool IsSecurityLevelAcceptableForWebAuthn(
      RenderFrameHost* rfh,
      const url::Origin& origin) override {
    return is_webauthn_security_level_acceptable;
  }

  std::unique_ptr<AuthenticatorRequestClientDelegate>
  GetWebAuthenticationRequestDelegate(
      RenderFrameHost* render_frame_host) override {
    return std::make_unique<AuthenticatorRequestClientDelegate>();
  }

  bool is_webauthn_security_level_acceptable = true;
};

class InternalAuthenticatorImplTest : public RenderViewHostTestHarness {
 protected:
  InternalAuthenticatorImplTest() = default;

  void SetUp() override {
    RenderViewHostTestHarness::SetUp();
    ResetVirtualDevice();
    old_client_ = SetBrowserClientForTesting(&test_client_);
  }

  void TearDown() override {
    internal_authenticator_impl_.reset();
    SetBrowserClientForTesting(old_client_);
    old_client_ = nullptr;
    virtual_device_factory_ = nullptr;
    scoped_authenticator_environment_.reset();
    RenderViewHostTestHarness::TearDown();
  }

  void NavigateAndCommit(const GURL& url) {
    internal_authenticator_impl_.reset();
    RenderViewHostTestHarness::NavigateAndCommit(url);
  }

  InternalAuthenticatorImpl* GetAuthenticator(
      const url::Origin& effective_origin_url) {
    internal_authenticator_impl_ =
        std::make_unique<InternalAuthenticatorImpl>(main_rfh());
    internal_authenticator_impl_->SetEffectiveOrigin(effective_origin_url);
    return internal_authenticator_impl_.get();
  }

  void ResetVirtualDevice() {
    virtual_device_factory_ = nullptr;
    scoped_authenticator_environment_.reset();
    auto factory = std::make_unique<device::test::VirtualFidoDeviceFactory>();
    virtual_device_factory_ = factory.get();
    scoped_authenticator_environment_ =
        std::make_unique<ScopedAuthenticatorEnvironmentForTesting>(
            std::move(factory));
  }

 protected:
  std::unique_ptr<InternalAuthenticatorImpl> internal_authenticator_impl_;
  TestAuthenticatorContentBrowserClient test_client_;
  raw_ptr<ContentBrowserClient> old_client_ = nullptr;
  raw_ptr<device::test::VirtualFidoDeviceFactory> virtual_device_factory_ =
      nullptr;
  std::unique_ptr<ScopedAuthenticatorEnvironmentForTesting>
      scoped_authenticator_environment_;
};

// Regression test for crbug.com/1433416.
TEST_F(InternalAuthenticatorImplTest, MakeCredentialSkipTLSCheck) {
  NavigateAndCommit(GURL(kTestOrigin1));
  InternalAuthenticatorImpl* authenticator =
      GetAuthenticator(url::Origin::Create(GURL(kTestOrigin1)));
  test_client_.is_webauthn_security_level_acceptable = false;
  PublicKeyCredentialCreationOptionsPtr options =
      GetTestPublicKeyCredentialCreationOptions();
  TestMakeCredentialFuture future;
  authenticator->MakeCredential(std::move(options), future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(std::get<0>(future.Get()),
            blink::mojom::AuthenticatorStatus::SUCCESS);
}

// Regression test for crbug.com/1433416.
TEST_F(InternalAuthenticatorImplTest, GetAssertionSkipTLSCheck) {
  NavigateAndCommit(GURL(kTestOrigin1));
  InternalAuthenticatorImpl* authenticator =
      GetAuthenticator(url::Origin::Create(GURL(kTestOrigin1)));
  test_client_.is_webauthn_security_level_acceptable = false;
  PublicKeyCredentialRequestOptionsPtr options =
      GetTestPublicKeyCredentialRequestOptions();
  ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
      options->allow_credentials[0].id, options->relying_party_id));
  TestGetAssertionFuture future;
  authenticator->GetAssertion(std::move(options), future.GetCallback());
  EXPECT_TRUE(future.Wait());
  EXPECT_EQ(std::get<0>(future.Get()),
            blink::mojom::AuthenticatorStatus::SUCCESS);
}

// Verify behavior for various combinations of origins and RP IDs.
TEST_F(InternalAuthenticatorImplTest, MakeCredentialOriginAndRpIds) {
  // These instances should return security errors (for circumstances
  // that would normally crash the renderer).
  for (auto test_case : webauthn::test::GetInvalidRpTestCases()) {
    SCOPED_TRACE(std::string(test_case.claimed_authority) + " " +
                 std::string(test_case.origin));

    GURL origin = GURL(test_case.origin);
    if (url::Origin::Create(origin).opaque()) {
      // Opaque origins will cause DCHECK to fail.
      continue;
    }

    NavigateAndCommit(origin);
    InternalAuthenticatorImpl* authenticator =
        GetAuthenticator(url::Origin::Create(origin));
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->relying_party.id = test_case.claimed_authority;
    TestMakeCredentialFuture future;
    authenticator->MakeCredential(std::move(options), future.GetCallback());
    EXPECT_TRUE(future.Wait());

    AuthenticatorStatus actual_status = std::get<0>(future.Get());
    if (test_case.expected_status ==
        AuthenticatorStatus::BAD_RELYING_PARTY_ID) {
      EXPECT_TRUE(
          actual_status == AuthenticatorStatus::BAD_RELYING_PARTY_ID ||
          actual_status ==
              AuthenticatorStatus::BAD_RELYING_PARTY_ID_ATTEMPTED_FETCH);
    } else {
      EXPECT_EQ(test_case.expected_status, actual_status);
    }
  }

  // These instances should bypass security errors, by setting the effective
  // origin to a valid one.
  for (auto test_case : webauthn::test::GetValidRpTestCases()) {
    SCOPED_TRACE(std::string(test_case.claimed_authority) + " " +
                 std::string(test_case.origin));

    NavigateAndCommit(GURL("https://this.isthewrong.origin"));
    auto* authenticator =
        GetAuthenticator(url::Origin::Create(GURL(test_case.origin)));
    PublicKeyCredentialCreationOptionsPtr options =
        GetTestPublicKeyCredentialCreationOptions();
    options->relying_party.id = test_case.claimed_authority;

    ResetVirtualDevice();
    TestMakeCredentialFuture future;
    authenticator->MakeCredential(std::move(options), future.GetCallback());
    EXPECT_TRUE(future.Wait());
    EXPECT_EQ(test_case.expected_status, std::get<0>(future.Get()));
  }
}

// Verify behavior for various combinations of origins and RP IDs.
TEST_F(InternalAuthenticatorImplTest, GetAssertionOriginAndRpIds) {
  // These instances should return security errors (for circumstances
  // that would normally crash the renderer).
  for (const webauthn::test::OriginClaimedAuthorityPair& test_case :
       webauthn::test::GetInvalidRpTestCases()) {
    SCOPED_TRACE(
        base::StrCat({test_case.claimed_authority, " ", test_case.origin}));

    GURL origin = GURL(test_case.origin);
    if (url::Origin::Create(origin).opaque()) {
      // Opaque origins will cause DCHECK to fail.
      continue;
    }

    NavigateAndCommit(origin);
    InternalAuthenticatorImpl* authenticator =
        GetAuthenticator(url::Origin::Create(origin));
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->relying_party_id = test_case.claimed_authority;

    TestGetAssertionFuture future;
    authenticator->GetAssertion(std::move(options), future.GetCallback());
    EXPECT_TRUE(future.Wait());

    AuthenticatorStatus actual_status = std::get<0>(future.Get());
    if (test_case.expected_status ==
        AuthenticatorStatus::BAD_RELYING_PARTY_ID) {
      EXPECT_TRUE(
          actual_status == AuthenticatorStatus::BAD_RELYING_PARTY_ID ||
          actual_status ==
              AuthenticatorStatus::BAD_RELYING_PARTY_ID_ATTEMPTED_FETCH);
    } else {
      EXPECT_EQ(test_case.expected_status, actual_status);
    }
  }

  // These instances should bypass security errors, by setting the effective
  // origin to a valid one.
  for (const webauthn::test::OriginClaimedAuthorityPair& test_case :
       webauthn::test::GetValidRpTestCases()) {
    SCOPED_TRACE(
        base::StrCat({test_case.claimed_authority, " ", test_case.origin}));

    NavigateAndCommit(GURL("https://this.isthewrong.origin"));
    InternalAuthenticatorImpl* authenticator =
        GetAuthenticator(url::Origin::Create(GURL(test_case.origin)));
    PublicKeyCredentialRequestOptionsPtr options =
        GetTestPublicKeyCredentialRequestOptions();
    options->relying_party_id = test_case.claimed_authority;

    ResetVirtualDevice();
    ASSERT_TRUE(virtual_device_factory_->mutable_state()->InjectRegistration(
        options->allow_credentials[0].id,
        std::string(test_case.claimed_authority)));
    TestGetAssertionFuture future;
    authenticator->GetAssertion(std::move(options), future.GetCallback());
    EXPECT_TRUE(future.Wait());
    EXPECT_EQ(test_case.expected_status, std::get<0>(future.Get()));
  }
}

}  // namespace content
