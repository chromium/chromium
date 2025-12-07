// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webauthn/content/browser/internal_authenticator_impl.h"

#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "content/browser/webauth/authenticator_test_base.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/common/content_client.h"
#include "content/public/test/navigation_simulator.h"
#include "device/fido/virtual_fido_device_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

namespace {

using blink::mojom::AuthenticationExtensionsClientInputs;
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

}  // namespace

class InternalAuthenticatorImplTest : public AuthenticatorTestBase {
 protected:
  InternalAuthenticatorImplTest() = default;

  void SetUp() override {
    AuthenticatorTestBase::SetUp();
    old_client_ = SetBrowserClientForTesting(&test_client_);
  }

  void TearDown() override {
    // The |RenderFrameHost| must outlive |AuthenticatorImpl|.
    internal_authenticator_impl_.reset();
    SetBrowserClientForTesting(old_client_);
    AuthenticatorTestBase::TearDown();
  }

  void NavigateAndCommit(const GURL& url) {
    // The |RenderFrameHost| must outlive |AuthenticatorImpl|.
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

 protected:
  std::unique_ptr<InternalAuthenticatorImpl> internal_authenticator_impl_;
  TestAuthenticatorContentBrowserClient test_client_;
  raw_ptr<ContentBrowserClient> old_client_ = nullptr;
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
  for (auto test_case : kInvalidRpTestCases) {
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
    EXPECT_EQ(test_case.expected_status, std::get<0>(future.Get()));
  }

  // These instances should bypass security errors, by setting the effective
  // origin to a valid one.
  for (auto test_case : kValidRpTestCases) {
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
  for (const OriginClaimedAuthorityPair& test_case : kInvalidRpTestCases) {
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
    EXPECT_EQ(test_case.expected_status, std::get<0>(future.Get()));
  }

  // These instances should bypass security errors, by setting the effective
  // origin to a valid one.
  for (const OriginClaimedAuthorityPair& test_case : kValidRpTestCases) {
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
