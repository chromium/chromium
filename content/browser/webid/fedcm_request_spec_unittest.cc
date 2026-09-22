// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/webid/fedcm_request_spec.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/test/gtest_util.h"
#include "base/test/scoped_feature_list.h"
#include "content/browser/webid/test/mock_permission_delegate.h"
#include "content/public/common/content_features.h"
#include "mojo/public/cpp/bindings/clone_traits.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/credentialmanagement/credential_manager.mojom.h"
#include "third_party/blink/public/mojom/webid/federated_request.mojom.h"
#include "url/gurl.h"

namespace content::webid {

using ::testing::NiceMock;
using ::testing::Return;

using FedCmRequestSpecTest = testing::Test;

TEST_F(FedCmRequestSpecTest, DefaultConstructor) {
  auto spec = base::MakeRefCounted<FedCmRequestSpec>();
  EXPECT_EQ(spec->rp_mode(), blink::mojom::RpMode::kPassive);
  EXPECT_EQ(spec->mediation_requirement(),
            password_manager::CredentialMediationRequirement::kOptional);
  EXPECT_EQ(spec->rp_context(), blink::mojom::RpContext::kSignIn);
  EXPECT_FALSE(spec->had_transient_user_activation());
  EXPECT_FALSE(spec->can_accept_redirect_to());
  EXPECT_TRUE(spec->intercepted_url().is_empty());
  EXPECT_TRUE(spec->idp_order().empty());
  EXPECT_TRUE(spec->idps_with_nonce().empty());
  EXPECT_TRUE(spec->idps_with_nonce_outside_params_only().empty());
  EXPECT_TRUE(spec->providers().empty());
}

TEST_F(FedCmRequestSpecTest, PassiveModeAndNonceTracking) {
  std::vector<blink::mojom::IdentityProviderRequestOptionsPtr> providers;

  // IdP 1: Nonce only outside params.
  auto idp1 = blink::mojom::IdentityProviderRequestOptions::New();
  idp1->config = blink::mojom::IdentityProviderConfig::New(
      GURL("https://idp1.example/fedcm.json"),
      /*from_idp_registration_api=*/false, /*type=*/std::nullopt,
      /*client_id=*/"");
  idp1->nonce = "nonce123";
  providers.push_back(std::move(idp1));

  // IdP 2: Nonce also inside params_json.
  auto idp2 = blink::mojom::IdentityProviderRequestOptions::New();
  idp2->config = blink::mojom::IdentityProviderConfig::New(
      GURL("https://idp2.example/fedcm.json"),
      /*from_idp_registration_api=*/false, /*type=*/std::nullopt,
      /*client_id=*/"");
  idp2->nonce = "nonce456";
  idp2->params_json = "{\"nonce\": \"nonce456\"}";
  providers.push_back(std::move(idp2));

  // IdP 3: No nonce.
  auto idp3 = blink::mojom::IdentityProviderRequestOptions::New();
  idp3->config = blink::mojom::IdentityProviderConfig::New(
      GURL("https://idp3.example/fedcm.json"),
      /*from_idp_registration_api=*/false, /*type=*/std::nullopt,
      /*client_id=*/"");
  providers.push_back(std::move(idp3));

  auto spec = FedCmRequestSpec::Build(
      /*rfh=*/nullptr, /*permission_delegate=*/nullptr,
      blink::mojom::IdentityProviderGetParameters::New(
          std::move(providers), blink::mojom::RpContext::kSignIn,
          blink::mojom::RpMode::kPassive),
      password_manager::CredentialMediationRequirement::kRequired,
      /*navigation_handle=*/nullptr,
      /*intercepted_url=*/GURL());

  EXPECT_EQ(spec->rp_mode(), blink::mojom::RpMode::kPassive);
  EXPECT_EQ(spec->mediation_requirement(),
            password_manager::CredentialMediationRequirement::kRequired);
  EXPECT_EQ(spec->rp_context(), blink::mojom::RpContext::kSignIn);
  EXPECT_FALSE(spec->had_transient_user_activation());
  EXPECT_FALSE(spec->can_accept_redirect_to());
  EXPECT_TRUE(spec->intercepted_url().is_empty());

  EXPECT_EQ((std::vector<GURL>{GURL("https://idp1.example/fedcm.json"),
                               GURL("https://idp2.example/fedcm.json"),
                               GURL("https://idp3.example/fedcm.json")}),
            spec->idp_order());

  EXPECT_TRUE(spec->idps_with_nonce().contains(
      GURL("https://idp1.example/fedcm.json")));
  EXPECT_TRUE(spec->idps_with_nonce().contains(
      GURL("https://idp2.example/fedcm.json")));
  EXPECT_FALSE(spec->idps_with_nonce().contains(
      GURL("https://idp3.example/fedcm.json")));

  EXPECT_TRUE(spec->idps_with_nonce_outside_params_only().contains(
      GURL("https://idp1.example/fedcm.json")));
  EXPECT_FALSE(spec->idps_with_nonce_outside_params_only().contains(
      GURL("https://idp2.example/fedcm.json")));
  EXPECT_EQ(spec->providers().size(), 3u);
}

// Verifies that for standard token requests without registration providers,
// Build() does not query the registered IdPs even if registration is enabled.
TEST_F(FedCmRequestSpecTest, BuildStandardRequestDoesNotQueryRegistry) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmIdPRegistration);

  NiceMock<MockPermissionDelegate> permission_delegate;
  EXPECT_CALL(permission_delegate, GetRegisteredIdPs()).Times(0);

  std::vector<blink::mojom::IdentityProviderRequestOptionsPtr> providers;

  auto idp1 = blink::mojom::IdentityProviderRequestOptions::New();
  idp1->config = blink::mojom::IdentityProviderConfig::New(
      GURL("https://idp1.example/fedcm.json"),
      /*from_idp_registration_api=*/false, /*type=*/std::nullopt,
      /*client_id=*/"");
  providers.push_back(std::move(idp1));

  auto idp2 = blink::mojom::IdentityProviderRequestOptions::New();
  idp2->config = blink::mojom::IdentityProviderConfig::New(
      GURL("https://idp2.example/fedcm.json"),
      /*from_idp_registration_api=*/false, /*type=*/std::nullopt,
      /*client_id=*/"");
  providers.push_back(std::move(idp2));

  auto expected_providers = mojo::Clone(providers);
  auto spec = FedCmRequestSpec::Build(
      /*rfh=*/nullptr, &permission_delegate,
      blink::mojom::IdentityProviderGetParameters::New(
          std::move(providers), blink::mojom::RpContext::kSignIn,
          blink::mojom::RpMode::kPassive),
      password_manager::CredentialMediationRequirement::kOptional);

  EXPECT_EQ(expected_providers, spec->providers());
  EXPECT_EQ((std::vector<GURL>{GURL("https://idp1.example/fedcm.json"),
                               GURL("https://idp2.example/fedcm.json")}),
            spec->idp_order());
}

// Verifies that when a registered IdP placeholder is present, Build() queries
// the registry and expands the registered IdPs inline in reverse chronological
// order while preserving named IdPs.
TEST_F(FedCmRequestSpecTest, BuildExpandsRegisteredIdPsInline) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmIdPRegistration);

  NiceMock<MockPermissionDelegate> permission_delegate;
  std::vector<GURL> registry = {GURL("https://registered1.example/fedcm.json"),
                                GURL("https://registered2.example/fedcm.json")};
  EXPECT_CALL(permission_delegate, GetRegisteredIdPs())
      .WillOnce(Return(registry));

  std::vector<blink::mojom::IdentityProviderRequestOptionsPtr> providers;

  auto named1 = blink::mojom::IdentityProviderRequestOptions::New();
  named1->config = blink::mojom::IdentityProviderConfig::New(
      GURL("https://named1.example/fedcm.json"),
      /*from_idp_registration_api=*/false, /*type=*/std::nullopt,
      /*client_id=*/"");
  providers.push_back(std::move(named1));

  auto registered = blink::mojom::IdentityProviderRequestOptions::New();
  registered->config = blink::mojom::IdentityProviderConfig::New(
      GURL(), /*from_idp_registration_api=*/true, /*type=*/std::nullopt,
      /*client_id=*/"");
  providers.push_back(std::move(registered));

  auto named2 = blink::mojom::IdentityProviderRequestOptions::New();
  named2->config = blink::mojom::IdentityProviderConfig::New(
      GURL("https://named2.example/fedcm.json"),
      /*from_idp_registration_api=*/false, /*type=*/std::nullopt,
      /*client_id=*/"");
  providers.push_back(std::move(named2));

  auto spec = FedCmRequestSpec::Build(
      /*rfh=*/nullptr, &permission_delegate,
      blink::mojom::IdentityProviderGetParameters::New(
          std::move(providers), blink::mojom::RpContext::kSignIn,
          blink::mojom::RpMode::kPassive),
      password_manager::CredentialMediationRequirement::kOptional);

  // Expect registered IdPs expanded in reverse order inline between named IdPs.
  std::vector<blink::mojom::IdentityProviderRequestOptionsPtr>
      expected_providers;

  auto expected_named1 = blink::mojom::IdentityProviderRequestOptions::New();
  expected_named1->config = blink::mojom::IdentityProviderConfig::New(
      GURL("https://named1.example/fedcm.json"),
      /*from_idp_registration_api=*/false, /*type=*/std::nullopt,
      /*client_id=*/"");
  expected_providers.push_back(std::move(expected_named1));

  auto expected_reg2 = blink::mojom::IdentityProviderRequestOptions::New();
  expected_reg2->config = blink::mojom::IdentityProviderConfig::New(
      GURL("https://registered2.example/fedcm.json"),
      /*from_idp_registration_api=*/true, /*type=*/std::nullopt,
      /*client_id=*/"");
  expected_providers.push_back(std::move(expected_reg2));

  auto expected_reg1 = blink::mojom::IdentityProviderRequestOptions::New();
  expected_reg1->config = blink::mojom::IdentityProviderConfig::New(
      GURL("https://registered1.example/fedcm.json"),
      /*from_idp_registration_api=*/true, /*type=*/std::nullopt,
      /*client_id=*/"");
  expected_providers.push_back(std::move(expected_reg1));

  auto expected_named2 = blink::mojom::IdentityProviderRequestOptions::New();
  expected_named2->config = blink::mojom::IdentityProviderConfig::New(
      GURL("https://named2.example/fedcm.json"),
      /*from_idp_registration_api=*/false, /*type=*/std::nullopt,
      /*client_id=*/"");
  expected_providers.push_back(std::move(expected_named2));

  EXPECT_EQ(expected_providers, spec->providers());
  EXPECT_EQ((std::vector<GURL>{GURL("https://named1.example/fedcm.json"),
                               GURL("https://registered2.example/fedcm.json"),
                               GURL("https://registered1.example/fedcm.json"),
                               GURL("https://named2.example/fedcm.json")}),
            spec->idp_order());
}

// Verifies that when a registered IdP placeholder is requested but the registry
// is empty, the placeholder expands to 0 IdPs and named IdPs remain.
TEST_F(FedCmRequestSpecTest, BuildRegisteredPlaceholderWithEmptyRegistry) {
  base::test::ScopedFeatureList list;
  list.InitAndEnableFeature(features::kFedCmIdPRegistration);

  NiceMock<MockPermissionDelegate> permission_delegate;
  EXPECT_CALL(permission_delegate, GetRegisteredIdPs())
      .WillOnce(Return(std::vector<GURL>()));

  std::vector<blink::mojom::IdentityProviderRequestOptionsPtr> providers;

  auto registered = blink::mojom::IdentityProviderRequestOptions::New();
  registered->config = blink::mojom::IdentityProviderConfig::New(
      GURL(), /*from_idp_registration_api=*/true, /*type=*/std::nullopt,
      /*client_id=*/"");
  providers.push_back(std::move(registered));

  auto named = blink::mojom::IdentityProviderRequestOptions::New();
  named->config = blink::mojom::IdentityProviderConfig::New(
      GURL("https://named.example/fedcm.json"),
      /*from_idp_registration_api=*/false, /*type=*/std::nullopt,
      /*client_id=*/"");
  providers.push_back(std::move(named));

  auto spec = FedCmRequestSpec::Build(
      /*rfh=*/nullptr, &permission_delegate,
      blink::mojom::IdentityProviderGetParameters::New(
          std::move(providers), blink::mojom::RpContext::kSignIn,
          blink::mojom::RpMode::kPassive),
      password_manager::CredentialMediationRequirement::kOptional);

  std::vector<blink::mojom::IdentityProviderRequestOptionsPtr>
      expected_providers;
  auto expected_named = blink::mojom::IdentityProviderRequestOptions::New();
  expected_named->config = blink::mojom::IdentityProviderConfig::New(
      GURL("https://named.example/fedcm.json"),
      /*from_idp_registration_api=*/false, /*type=*/std::nullopt,
      /*client_id=*/"");
  expected_providers.push_back(std::move(expected_named));

  EXPECT_EQ(expected_providers, spec->providers());
  EXPECT_EQ(std::vector<GURL>{GURL("https://named.example/fedcm.json")},
            spec->idp_order());
}

TEST_F(FedCmRequestSpecTest, BuildAssertsOnNullIdpGetParams) {
  EXPECT_CHECK_DEATH(FedCmRequestSpec::Build(
      /*rfh=*/nullptr, /*permission_delegate=*/nullptr,
      /*idp_get_params=*/nullptr,
      password_manager::CredentialMediationRequirement::kOptional));
}

}  // namespace content::webid
