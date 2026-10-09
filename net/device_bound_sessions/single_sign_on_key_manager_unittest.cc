// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/single_sign_on_key_manager.h"

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_expected_support.h"
#include "base/types/expected.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "net/base/schemeful_site.h"
#include "net/device_bound_sessions/cookie_access_check_params.h"
#include "net/device_bound_sessions/registration_fetcher_param.h"
#include "net/device_bound_sessions/session.h"
#include "net/device_bound_sessions/session_error.h"
#include "net/device_bound_sessions/session_params.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net::device_bound_sessions {

namespace {

SessionParams CreateTestSessionParams() {
  return SessionParams{
      .session_id = "test_session",
      .fetcher_url = GURL("https://rp.test/refresh"),
      .refresh_url = "https://rp.test/refresh",
      .scope = {.include_site = true, .origin = "https://rp.test"},
      .credentials = {{.name = "test_cookie", .attributes = "secure"}},
  };
}

class SingleSignOnKeyManagerTest : public testing::Test {
 public:
  SingleSignOnKeyManagerTest() = default;

  void SetUp() override {
    manager_ = std::make_unique<SingleSignOnKeyManager>(
        base::BindLambdaForTesting([&](const CookieAccessCheckParams& params) {
          return allow_cookie_access_;
        }));
  }

  void SetCookieAccess(bool allow) { allow_cookie_access_ = allow; }

  SingleSignOnKeyManager& manager() { return *manager_; }

 private:
  bool allow_cookie_access_ = true;
  std::unique_ptr<SingleSignOnKeyManager> manager_;
};

TEST_F(SingleSignOnKeyManagerTest,
       KeyIsAccessibleByCorrectRelyingPartyWithCorrectKeyDigest) {
  auto rp_origin = url::Origin::Create(GURL("https://rp.test"));
  std::string provider_key = "key_digest_123";
  GURL provider_url("https://provider.test");

  unexportable_keys::UnexportableSigningKeyId key_id;

  EXPECT_TRUE(manager().AddPreProvisionedKey(rp_origin, provider_key,
                                             provider_url, key_id));

  SessionErrorOr<unexportable_keys::UnexportableSigningKeyId> found_key =
      manager().FindPreProvisionedKey(
          ProviderRegistrationParams{.provider_key = provider_key,
                                     .provider_url = provider_url},
          rp_origin);

  EXPECT_THAT(found_key, base::test::ValueIs(key_id));
}

TEST_F(SingleSignOnKeyManagerTest, KeyIsNotAccessibleByWrongRelyingParty) {
  auto rp_origin = url::Origin::Create(GURL("https://rp.test"));
  std::string provider_key = "key_digest_123";
  GURL provider_url("https://provider.test");

  unexportable_keys::UnexportableSigningKeyId key_id;

  EXPECT_TRUE(manager().AddPreProvisionedKey(rp_origin, provider_key,
                                             provider_url, key_id));

  // Wrong RP (relying party mismatch): key is not accessible.
  auto wrong_rp_origin = url::Origin::Create(GURL("https://wrong-rp.test"));
  EXPECT_THAT(manager().FindPreProvisionedKey(
                  ProviderRegistrationParams{.provider_key = provider_key,
                                             .provider_url = provider_url},
                  wrong_rp_origin),
              base::test::ErrorIs(SessionError::kPreProvisionedKeyNotFound));

  // Wrong IdP (identity provider mismatch): key is not accessible.
  GURL wrong_provider_url("https://wrong-provider.test");

  EXPECT_THAT(
      manager().FindPreProvisionedKey(
          ProviderRegistrationParams{.provider_key = provider_key,
                                     .provider_url = wrong_provider_url},
          rp_origin),
      base::test::ErrorIs(SessionError::kPreProvisionedKeyNotFound));

  // Wrong key digest (provider key mismatch): key is not accessible.
  std::string wrong_provider_key = "wrong_digest_456";
  EXPECT_THAT(manager().FindPreProvisionedKey(
                  ProviderRegistrationParams{.provider_key = wrong_provider_key,
                                             .provider_url = provider_url},
                  rp_origin),
              base::test::ErrorIs(SessionError::kPreProvisionedKeyNotFound));
}

TEST_F(SingleSignOnKeyManagerTest, NoCookieAccess) {
  auto rp_origin = url::Origin::Create(GURL("https://example.test"));
  std::string provider_key = "123";
  GURL provider_url("https://provider.test");

  SetCookieAccess(false);
  EXPECT_FALSE(manager().AddPreProvisionedKey(
      rp_origin, provider_key, provider_url,
      unexportable_keys::UnexportableSigningKeyId()));
}

TEST_F(SingleSignOnKeyManagerTest,
       FindPreProvisionedKey_CookieAccessRevoked_ReturnsAccessNotGranted) {
  auto rp_origin = url::Origin::Create(GURL("https://rp.test"));
  GURL provider_url("https://provider.test");

  EXPECT_TRUE(manager().AddPreProvisionedKey(
      rp_origin, "key1", provider_url,
      unexportable_keys::UnexportableSigningKeyId()));

  SetCookieAccess(false);
  EXPECT_THAT(
      manager().FindPreProvisionedKey(
          ProviderRegistrationParams{.provider_key = "key1",
                                     .provider_url = provider_url},
          rp_origin),
      base::test::ErrorIs(SessionError::kPreProvisionedKeyAccessNotGranted));
}

TEST_F(SingleSignOnKeyManagerTest, MissingInitiator) {
  auto rp_origin = url::Origin::Create(GURL("https://example.test"));
  std::string provider_key = "123";
  GURL provider_url("https://provider.test");

  EXPECT_TRUE(manager().AddPreProvisionedKey(
      rp_origin, provider_key, provider_url,
      unexportable_keys::UnexportableSigningKeyId()));

  SessionErrorOr<unexportable_keys::UnexportableSigningKeyId> found_key =
      manager().FindPreProvisionedKey(
          ProviderRegistrationParams{.provider_key = provider_key,
                                     .provider_url = provider_url},
          /*initiator=*/std::nullopt);

  EXPECT_THAT(found_key,
              base::test::ErrorIs(
                  SessionError::kInvalidPreProvisionedKeyInitiatorMissing));
}

TEST_F(SingleSignOnKeyManagerTest, MultipleKeysLimit) {
  auto rp_origin = url::Origin::Create(GURL("https://example.test"));
  GURL provider_url("https://provider.test");

  const size_t kNumKeysToGenerate =
      SingleSignOnKeyManager::kMaxPreProvisionedKeysPerIdentityProvider + 1;

  for (size_t i = 0; i < kNumKeysToGenerate; ++i) {
    std::string provider_key = base::NumberToString(i);
    bool should_add_key =
        i < SingleSignOnKeyManager::kMaxPreProvisionedKeysPerIdentityProvider;

    EXPECT_EQ(manager().AddPreProvisionedKey(
                  rp_origin, provider_key, provider_url,
                  unexportable_keys::UnexportableSigningKeyId()),
              should_add_key);
  }
}

TEST_F(SingleSignOnKeyManagerTest, MultipleKeysLimitSharedAcrossRps) {
  // Test that keys created for *different relying parties* but the
  // *same Identity Provider* share the same IDP limit and will eventually
  // hit the roof.
  GURL provider_url("https://company.idp.test/company");

  const size_t kNumKeysToGenerate =
      SingleSignOnKeyManager::kMaxPreProvisionedKeysPerIdentityProvider + 1;

  for (size_t i = 0; i < kNumKeysToGenerate; ++i) {
    std::string provider_key = base::NumberToString(i);
    // Vary the RP origin.
    auto iter_rp_origin = url::Origin::Create(GURL(
        base::StrCat({"https://sub", base::NumberToString(i), ".rp.test"})));

    bool should_add_key =
        i < SingleSignOnKeyManager::kMaxPreProvisionedKeysPerIdentityProvider;
    EXPECT_EQ(manager().AddPreProvisionedKey(
                  iter_rp_origin, provider_key, provider_url,
                  unexportable_keys::UnexportableSigningKeyId()),
              should_add_key);
  }
}

TEST_F(SingleSignOnKeyManagerTest, MultipleKeysLimitSharedAcrossIdpSubdomains) {
  // Test that keys created for the *same Relying Party* but Identity
  // Providers varying subdomains and paths share the same limit.
  auto rp_origin = url::Origin::Create(GURL("https://rp.test"));

  const size_t kNumKeysToGenerate =
      SingleSignOnKeyManager::kMaxPreProvisionedKeysPerIdentityProvider + 1;

  for (size_t i = 0; i < kNumKeysToGenerate; ++i) {
    std::string provider_key = base::NumberToString(i);

    // Vary the IDP URL using subdomains and paths.
    GURL provider_url;
    if (i % 2 == 0) {
      provider_url = GURL(absl::StrFormat("https://company%d.idp.test", i));
    } else {
      provider_url = GURL(absl::StrFormat("https://idp.test/company%d", i));
    }

    bool should_add_key =
        i < SingleSignOnKeyManager::kMaxPreProvisionedKeysPerIdentityProvider;
    EXPECT_EQ(manager().AddPreProvisionedKey(
                  rp_origin, provider_key, provider_url,
                  unexportable_keys::UnexportableSigningKeyId()),
              should_add_key);
  }
}

TEST_F(SingleSignOnKeyManagerTest,
       ClearPreProvisionedKeys_NullMatcher_ClearsAllKeys) {
  auto rp_origin = url::Origin::Create(GURL("https://rp.test"));
  GURL provider_url("https://provider.test");

  EXPECT_TRUE(manager().AddPreProvisionedKey(
      rp_origin, "key1", provider_url,
      unexportable_keys::UnexportableSigningKeyId()));
  manager().ClearPreProvisionedKeys(base::NullCallback());

  EXPECT_THAT(manager().FindPreProvisionedKey(
                  ProviderRegistrationParams{.provider_key = "key1",
                                             .provider_url = provider_url},
                  rp_origin),
              base::test::ErrorIs(SessionError::kPreProvisionedKeyNotFound));
}

TEST_F(SingleSignOnKeyManagerTest,
       ClearPreProvisionedKeys_Matcher_ClearsMatchingRpKeysOnly) {
  auto rp1_origin = url::Origin::Create(GURL("https://rp1.test"));
  auto rp2_origin = url::Origin::Create(GURL("https://rp2.test"));
  GURL provider_url("https://provider.test");
  unexportable_keys::UnexportableSigningKeyId key1_id;
  unexportable_keys::UnexportableSigningKeyId key2_id;

  EXPECT_TRUE(manager().AddPreProvisionedKey(rp1_origin, "key1", provider_url,
                                             key1_id));
  EXPECT_TRUE(manager().AddPreProvisionedKey(rp2_origin, "key2", provider_url,
                                             key2_id));

  manager().ClearPreProvisionedKeys(base::BindRepeating(
      [](const url::Origin& origin, const SchemefulSite& site) {
        return origin.host() == "rp1.test";
      }));

  EXPECT_THAT(manager().FindPreProvisionedKey(
                  ProviderRegistrationParams{.provider_key = "key1",
                                             .provider_url = provider_url},
                  rp1_origin),
              base::test::ErrorIs(SessionError::kPreProvisionedKeyNotFound));
  EXPECT_THAT(manager().FindPreProvisionedKey(
                  ProviderRegistrationParams{.provider_key = "key2",
                                             .provider_url = provider_url},
                  rp2_origin),
              base::test::ValueIs(key2_id));
}

TEST_F(SingleSignOnKeyManagerTest, ConsumeKeyForSession_RemovesMatchingKey) {
  auto rp_origin = url::Origin::Create(GURL("https://rp.test"));
  GURL provider_url("https://provider.test");
  unexportable_keys::UnexportableSigningKeyId key_id;

  EXPECT_TRUE(
      manager().AddPreProvisionedKey(rp_origin, "key1", provider_url, key_id));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(CreateTestSessionParams()));
  session->set_unexportable_key_id(key_id);

  manager().ConsumeKeyForSession(*session);

  EXPECT_THAT(manager().FindPreProvisionedKey(
                  ProviderRegistrationParams{.provider_key = "key1",
                                             .provider_url = provider_url},
                  rp_origin),
              base::test::ErrorIs(SessionError::kPreProvisionedKeyNotFound));
}

TEST_F(SingleSignOnKeyManagerTest,
       ConsumeKeyForSession_NonMatchingKeyId_KeepsKeys) {
  auto rp_origin = url::Origin::Create(GURL("https://rp.test"));
  GURL provider_url("https://provider.test");
  unexportable_keys::UnexportableSigningKeyId key_id;

  EXPECT_TRUE(
      manager().AddPreProvisionedKey(rp_origin, "key1", provider_url, key_id));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(CreateTestSessionParams()));
  session->set_unexportable_key_id(
      unexportable_keys::UnexportableSigningKeyId());

  manager().ConsumeKeyForSession(*session);

  EXPECT_THAT(manager().FindPreProvisionedKey(
                  ProviderRegistrationParams{.provider_key = "key1",
                                             .provider_url = provider_url},
                  rp_origin),
              base::test::ValueIs(key_id));
}

TEST_F(SingleSignOnKeyManagerTest, ConsumeKeyForSession_NoKeyId_KeepsKeys) {
  auto rp_origin = url::Origin::Create(GURL("https://rp.test"));
  GURL provider_url("https://provider.test");
  unexportable_keys::UnexportableSigningKeyId key_id;

  EXPECT_TRUE(
      manager().AddPreProvisionedKey(rp_origin, "key1", provider_url, key_id));

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Session> session,
                       Session::CreateIfValid(CreateTestSessionParams()));
  session->set_unexportable_key_id(
      base::unexpected(unexportable_keys::ServiceError::kKeyNotReady));

  manager().ConsumeKeyForSession(*session);

  EXPECT_THAT(manager().FindPreProvisionedKey(
                  ProviderRegistrationParams{.provider_key = "key1",
                                             .provider_url = provider_url},
                  rp_origin),
              base::test::ValueIs(key_id));
}

TEST_F(SingleSignOnKeyManagerTest,
       AddPreProvisionedKey_DuplicateKey_ReturnsFalse) {
  auto rp_origin = url::Origin::Create(GURL("https://rp.test"));
  GURL provider_url("https://provider.test");

  EXPECT_TRUE(manager().AddPreProvisionedKey(
      rp_origin, "key1", provider_url,
      unexportable_keys::UnexportableSigningKeyId()));
  EXPECT_FALSE(manager().AddPreProvisionedKey(
      rp_origin, "key1", provider_url,
      unexportable_keys::UnexportableSigningKeyId()));
}

TEST_F(SingleSignOnKeyManagerTest,
       AddPreProvisionedKey_OtherIdpAtLimit_ReturnsTrue) {
  auto rp_origin = url::Origin::Create(GURL("https://rp.test"));
  GURL full_provider_url("https://full-idp.test");
  GURL other_provider_url("https://other-idp.test");

  for (size_t i = 0;
       i < SingleSignOnKeyManager::kMaxPreProvisionedKeysPerIdentityProvider;
       ++i) {
    EXPECT_TRUE(manager().AddPreProvisionedKey(
        rp_origin, base::NumberToString(i), full_provider_url,
        unexportable_keys::UnexportableSigningKeyId()));
  }

  EXPECT_TRUE(manager().AddPreProvisionedKey(
      rp_origin, "other", other_provider_url,
      unexportable_keys::UnexportableSigningKeyId()));
}

}  // namespace
}  // namespace net::device_bound_sessions
