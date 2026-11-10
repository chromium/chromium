// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/device_bound_sessions/single_sign_on_key_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/base64url.h"
#include "base/cancelable_callback.h"
#include "base/containers/span.h"
#include "base/containers/to_vector.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_view_util.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/unexportable_keys/background_task_origin.h"
#include "components/unexportable_keys/mock_unexportable_key_service.h"
#include "components/unexportable_keys/service_error.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "components/unexportable_keys/unexportable_key_service.h"
#include "components/unexportable_keys/unexportable_key_service_impl.h"
#include "components/unexportable_keys/unexportable_key_task_manager.h"
#include "crypto/scoped_fake_unexportable_key_provider.h"
#include "crypto/sign.h"
#include "net/base/schemeful_site.h"
#include "net/device_bound_sessions/cookie_access_check_params.h"
#include "net/device_bound_sessions/generate_key_param.h"
#include "net/device_bound_sessions/jwk_utils.h"
#include "net/device_bound_sessions/session.h"
#include "net/device_bound_sessions/session_error.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace net::device_bound_sessions {

namespace {

using ::base::test::ErrorIs;
using ::base::test::HasValue;
using ::base::test::TestFuture;
using ::base::test::ValueIs;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::SizeIs;
using ::testing::UnorderedElementsAre;

url::Origin ProviderOrigin() {
  return url::Origin::Create(GURL("https://provider.test"));
}

// Returns a valid param for a GenerateKey request for https://rp.test.
GenerateKeyParam CreateGenerateKeyParam() {
  return GenerateKeyParam{
      .supported_algos = {crypto::sign::ECDSA_SHA256},
      .target_origin = url::Origin::Create(GURL("https://rp.test")),
      .provider_session_id = Session::Id("SessionWithAik"),
      .challenge = "test_challenge",
  };
}

// An RP key generated through the key service, captured before the manager
// sees it.
struct CapturedRpKey {
  unexportable_keys::UnexportableSigningKeyId id;
  std::string thumbprint;
};

class SingleSignOnKeyManagerTest : public testing::Test {
 public:
  SingleSignOnKeyManagerTest() {
    mock_key_service_.DelegateToService(unexportable_key_service_);
  }

  ~SingleSignOnKeyManagerTest() override {
    // The expectations' actions capture locals of the test body by reference.
    // Clear them before `manager_` is destroyed, which may still call into the
    // mock (e.g. to delete remaining stored keys or the key of a pending
    // request).
    testing::Mock::VerifyAndClear(&mock_key_service_);
  }

  void SetCookieAccess(bool allow) { allow_cookie_access_ = allow; }

  // Grants cookie access to `provider_origin` only.
  void SetCookieAccessOnlyFor(const url::Origin& provider_origin) {
    allowed_provider_origin_ = provider_origin;
  }

  // The provider origin of the most recent cookie access check, if any.
  const std::optional<url::Origin>& last_cookie_access_provider_origin() const {
    return last_cookie_access_provider_origin_;
  }

  void ClearLastCookieAccessProviderOrigin() {
    last_cookie_access_provider_origin_.reset();
  }

  SingleSignOnKeyManager& manager() { return *manager_; }
  void reset_manager() { manager_.reset(); }

  unexportable_keys::MockUnexportableKeyService& mock_key_service() {
    return mock_key_service_;
  }

  // The IdP's attestation key, generated when the fixture is created.
  unexportable_keys::UnexportableAttestationKeyId aik_id() const {
    return aik_id_;
  }

  // Forwards the next `GenerateSigningKeySlowlyAsync()` call to the real key
  // service and fulfills `generated` with the new key. The thumbprint is
  // computed right away, so it stays available after the key was deleted.
  void ExpectRpKeyGenerated(TestFuture<CapturedRpKey>& generated) {
    EXPECT_CALL(mock_key_service_, GenerateSigningKeySlowlyAsync)
        .WillOnce([this, &generated](
                      base::span<const crypto::sign::SignatureKind>
                          acceptable_algorithms,
                      unexportable_keys::BackgroundTaskPriority priority,
                      base::OnceCallback<void(
                          unexportable_keys::ServiceErrorOr<
                              unexportable_keys::UnexportableSigningKeyId>)>
                          callback) {
          unexportable_key_service_.GenerateSigningKeySlowlyAsync(
              acceptable_algorithms, priority,
              base::BindLambdaForTesting(
                  [this, &generated, callback = std::move(callback)](
                      unexportable_keys::ServiceErrorOr<
                          unexportable_keys::UnexportableSigningKeyId>
                          result) mutable {
                    ASSERT_OK_AND_ASSIGN(
                        unexportable_keys::UnexportableSigningKeyId id, result);
                    ASSERT_OK_AND_ASSIGN(
                        std::string thumbprint,
                        GetJwkThumbprint(unexportable_key_service_, id));
                    generated.SetValue(CapturedRpKey{
                        .id = id, .thumbprint = std::move(thumbprint)});
                    std::move(callback).Run(std::move(result));
                  }));
        });
  }

  // Forwards the next `DeleteKeysSlowlyAsync()` call to the real key service
  // and fulfills `deleted` when it happens.
  void ExpectRpKeyDeleted(TestFuture<void>& deleted) {
    EXPECT_CALL(mock_key_service_, DeleteKeysSlowlyAsync)
        .WillOnce(
            [this, &deleted](
                base::span<const unexportable_keys::UnexportableSigningKeyId>
                    key_ids,
                unexportable_keys::BackgroundTaskPriority priority,
                base::OnceCallback<void(
                    unexportable_keys::ServiceErrorOr<size_t>)> callback) {
              unexportable_key_service_.DeleteKeysSlowlyAsync(
                  key_ids, priority, std::move(callback));
              deleted.SetValue();
            });
  }

  // Holds the next `CertifySlowlyAsync()` call and fulfills `held` with a
  // closure that forwards the call to the real key service.
  void HoldCertify(TestFuture<base::OnceClosure>& held) {
    EXPECT_CALL(mock_key_service_, CertifySlowlyAsync)
        .WillOnce([this, &held](
                      unexportable_keys::UnexportableAttestationKeyId aik_id,
                      unexportable_keys::UnexportableSigningKeyId key_id,
                      base::span<const uint8_t> challenge,
                      unexportable_keys::BackgroundTaskPriority priority,
                      base::OnceCallback<void(unexportable_keys::ServiceErrorOr<
                                              crypto::AttestationStatement>)>
                          callback) {
          held.SetValue(base::BindLambdaForTesting(
              [this, aik_id, key_id, priority,
               challenge = base::ToVector(challenge),
               callback = std::move(callback)]() mutable {
                unexportable_key_service_.CertifySlowlyAsync(
                    aik_id, key_id, challenge, priority, std::move(callback));
              }));
        });
  }

  // Looks up the pre-provisioned key that a request for
  // `CreateGenerateKeyParam()` and https://provider.test stored for `key`.
  SessionErrorOr<unexportable_keys::UnexportableSigningKeyId> FindGeneratedKey(
      const CapturedRpKey& key) {
    return manager().FindPreProvisionedKey(
        key.thumbprint, CreateGenerateKeyParam().target_origin);
  }

  // Returns a real attestation key. A default-constructed ID would not do,
  // because `CertifySlowlyAsync()` fails with `kKeyNotFound` for unknown keys.
  unexportable_keys::UnexportableAttestationKeyId GenerateAttestationKey() {
    TestFuture<unexportable_keys::ServiceErrorOr<
        unexportable_keys::UnexportableAttestationKeyId>>
        key_future;
    unexportable_key_service_.GenerateAttestationKeySlowlyAsync(
        {crypto::sign::ECDSA_SHA256},
        unexportable_keys::BackgroundTaskPriority::kBestEffort,
        key_future.GetCallback());
    return *key_future.Take();
  }

  unexportable_keys::UnexportableKeyService& key_service() {
    return unexportable_key_service_;
  }

 private:
  base::test::TaskEnvironment task_environment_;
  bool allow_cookie_access_ = true;
  std::optional<url::Origin> allowed_provider_origin_;
  std::optional<url::Origin> last_cookie_access_provider_origin_;
  // Installed before the key service, which may create keys in its
  // constructor (e.g. for its spare key pools).
  crypto::ScopedFakeUnexportableKeyProvider scoped_fake_key_provider_;
  unexportable_keys::UnexportableKeyTaskManager task_manager_;
  unexportable_keys::UnexportableKeyServiceImpl unexportable_key_service_{
      task_manager_,
      unexportable_keys::BackgroundTaskOrigin::kDeviceBoundSessionCredentials,
      crypto::UnexportableKeyProvider::Config()};
  testing::NiceMock<unexportable_keys::MockUnexportableKeyService>
      mock_key_service_;
  unexportable_keys::UnexportableAttestationKeyId aik_id_ =
      GenerateAttestationKey();
  std::unique_ptr<SingleSignOnKeyManager> manager_ =
      std::make_unique<SingleSignOnKeyManager>(
          mock_key_service_,
          base::BindLambdaForTesting(
              [&](const CookieAccessCheckParams& params) {
                last_cookie_access_provider_origin_ = *params.provider_origin;
                return allow_cookie_access_ &&
                       (!allowed_provider_origin_ ||
                        *allowed_provider_origin_ == *params.provider_origin);
              }));
};

TEST_F(SingleSignOnKeyManagerTest,
       KeyIsAccessibleByCorrectRelyingPartyWithCorrectKeyDigest) {
  auto rp_origin = url::Origin::Create(GURL("https://rp.test"));
  std::string provider_key = "key_digest_123";
  auto provider_origin = ProviderOrigin();

  unexportable_keys::UnexportableSigningKeyId key_id;

  EXPECT_TRUE(manager().AddPreProvisionedKey(rp_origin, provider_key,
                                             provider_origin, key_id));

  SessionErrorOr<unexportable_keys::UnexportableSigningKeyId> found_key =
      manager().FindPreProvisionedKey(provider_key, rp_origin);

  EXPECT_THAT(found_key, ValueIs(key_id));
}

TEST_F(SingleSignOnKeyManagerTest, KeyIsNotAccessibleByWrongRelyingParty) {
  auto rp_origin = url::Origin::Create(GURL("https://rp.test"));
  std::string provider_key = "key_digest_123";
  auto provider_origin = ProviderOrigin();

  unexportable_keys::UnexportableSigningKeyId key_id;

  EXPECT_TRUE(manager().AddPreProvisionedKey(rp_origin, provider_key,
                                             provider_origin, key_id));

  // Wrong RP (relying party mismatch): key is not accessible.
  auto wrong_rp_origin = url::Origin::Create(GURL("https://wrong-rp.test"));
  EXPECT_THAT(manager().FindPreProvisionedKey(provider_key, wrong_rp_origin),
              ErrorIs(SessionError::kPreProvisionedKeyNotFound));

  // Wrong key digest (provider key mismatch): key is not accessible.
  std::string wrong_provider_key = "wrong_digest_456";
  EXPECT_THAT(manager().FindPreProvisionedKey(wrong_provider_key, rp_origin),
              ErrorIs(SessionError::kPreProvisionedKeyNotFound));
}

TEST_F(SingleSignOnKeyManagerTest, KeysAreScopedToRpOrigin) {
  auto rp_origin = url::Origin::Create(GURL("https://rp.test"));
  // Same site as `rp_origin`, but different origins.
  auto subdomain_origin = url::Origin::Create(GURL("https://sub.rp.test"));
  auto port_origin = url::Origin::Create(GURL("https://rp.test:8443"));
  unexportable_keys::UnexportableSigningKeyId rp_key_id;
  unexportable_keys::UnexportableSigningKeyId subdomain_key_id;

  EXPECT_TRUE(manager().AddPreProvisionedKey(rp_origin, "key", ProviderOrigin(),
                                             rp_key_id));
  EXPECT_THAT(manager().FindPreProvisionedKey("key", subdomain_origin),
              ErrorIs(SessionError::kPreProvisionedKeyNotFound));
  EXPECT_THAT(manager().FindPreProvisionedKey("key", port_origin),
              ErrorIs(SessionError::kPreProvisionedKeyNotFound));

  // A same-site origin can store a key with the same thumbprint.
  EXPECT_TRUE(manager().AddPreProvisionedKey(
      subdomain_origin, "key", ProviderOrigin(), subdomain_key_id));
  EXPECT_THAT(manager().FindPreProvisionedKey("key", rp_origin),
              ValueIs(rp_key_id));
  EXPECT_THAT(manager().FindPreProvisionedKey("key", subdomain_origin),
              ValueIs(subdomain_key_id));
}

TEST_F(SingleSignOnKeyManagerTest,
       FindPreProvisionedKeyChecksCookieAccessOfGeneratingIdp) {
  auto rp_origin = url::Origin::Create(GURL("https://rp.test"));
  auto other_provider_origin =
      url::Origin::Create(GURL("https://other-provider.test"));
  unexportable_keys::UnexportableSigningKeyId key_id;
  EXPECT_TRUE(manager().AddPreProvisionedKey(rp_origin, "key1",
                                             ProviderOrigin(), key_id));
  EXPECT_TRUE(manager().AddPreProvisionedKey(
      rp_origin, "key2", other_provider_origin,
      unexportable_keys::UnexportableSigningKeyId()));

  // SSO registrations don't name the IdP (WICG/dbsc-sso#18), so the cookie
  // access check uses the IdP that generated the key.
  SetCookieAccessOnlyFor(ProviderOrigin());
  EXPECT_THAT(manager().FindPreProvisionedKey("key1", rp_origin),
              ValueIs(key_id));
  EXPECT_THAT(manager().FindPreProvisionedKey("key2", rp_origin),
              ErrorIs(SessionError::kPreProvisionedKeyAccessNotGranted));
}

TEST_F(SingleSignOnKeyManagerTest,
       FindPreProvisionedKeyWithoutMatchOrCookieAccessReturnsNotFound) {
  auto rp_origin = url::Origin::Create(GURL("https://rp.test"));
  EXPECT_TRUE(manager().AddPreProvisionedKey(
      rp_origin, "key1", ProviderOrigin(),
      unexportable_keys::UnexportableSigningKeyId()));
  SetCookieAccess(false);
  ClearLastCookieAccessProviderOrigin();

  // Without a matching key there is no IdP to check cookie access for, so the
  // lookup fails before the cookie access check.
  EXPECT_THAT(manager().FindPreProvisionedKey("other_key", rp_origin),
              ErrorIs(SessionError::kPreProvisionedKeyNotFound));
  EXPECT_EQ(last_cookie_access_provider_origin(), std::nullopt);
}

TEST_F(SingleSignOnKeyManagerTest, NoCookieAccess) {
  auto rp_origin = url::Origin::Create(GURL("https://example.test"));
  std::string provider_key = "123";
  auto provider_origin = ProviderOrigin();

  SetCookieAccess(false);
  EXPECT_FALSE(manager().AddPreProvisionedKey(
      rp_origin, provider_key, provider_origin,
      unexportable_keys::UnexportableSigningKeyId()));
}

TEST_F(SingleSignOnKeyManagerTest,
       FindPreProvisionedKey_CookieAccessRevoked_ReturnsAccessNotGranted) {
  auto rp_origin = url::Origin::Create(GURL("https://rp.test"));
  auto provider_origin = ProviderOrigin();

  EXPECT_TRUE(manager().AddPreProvisionedKey(
      rp_origin, "key1", provider_origin,
      unexportable_keys::UnexportableSigningKeyId()));

  SetCookieAccess(false);
  EXPECT_THAT(manager().FindPreProvisionedKey("key1", rp_origin),
              ErrorIs(SessionError::kPreProvisionedKeyAccessNotGranted));
}

TEST_F(SingleSignOnKeyManagerTest, MissingInitiator) {
  auto rp_origin = url::Origin::Create(GURL("https://example.test"));
  std::string provider_key = "123";
  auto provider_origin = ProviderOrigin();

  EXPECT_TRUE(manager().AddPreProvisionedKey(
      rp_origin, provider_key, provider_origin,
      unexportable_keys::UnexportableSigningKeyId()));

  SessionErrorOr<unexportable_keys::UnexportableSigningKeyId> found_key =
      manager().FindPreProvisionedKey(provider_key, /*initiator=*/std::nullopt);

  EXPECT_THAT(found_key,
              ErrorIs(SessionError::kInvalidPreProvisionedKeyInitiatorMissing));
}

TEST_F(SingleSignOnKeyManagerTest, AddPreProvisionedKeyRecordsCount) {
  auto rp_origin = url::Origin::Create(GURL("https://example.test"));
  auto other_rp_origin = url::Origin::Create(GURL("https://other-rp.test"));
  auto provider_origin = ProviderOrigin();

  // Keys for one IdP aren't capped.
  constexpr size_t kNumKeysToGenerate = 15;
  for (size_t i = 0; i < kNumKeysToGenerate; ++i) {
    base::HistogramTester step_histogram_tester;
    EXPECT_TRUE(manager().CanGenerateKey(provider_origin, rp_origin));
    EXPECT_TRUE(manager().AddPreProvisionedKey(
        rp_origin, base::NumberToString(i), provider_origin,
        unexportable_keys::UnexportableSigningKeyId()));
    step_histogram_tester.ExpectUniqueSample(
        "Net.DeviceBoundSessions.PreProvisionedKeyCount", i + 1, 1);
  }
  EXPECT_TRUE(manager().CanGenerateKey(provider_origin, rp_origin));

  EXPECT_TRUE(manager().AddPreProvisionedKey(
      other_rp_origin, "other_key", provider_origin,
      unexportable_keys::UnexportableSigningKeyId()));

  {
    base::HistogramTester duplicate_histogram_tester;
    EXPECT_FALSE(manager().AddPreProvisionedKey(
        rp_origin, "0", provider_origin,
        unexportable_keys::UnexportableSigningKeyId()));
    duplicate_histogram_tester.ExpectTotalCount(
        "Net.DeviceBoundSessions.PreProvisionedKeyCount", 0);
  }

  manager().ClearPreProvisionedKeys(base::BindRepeating(
      [](const url::Origin& rp_origin, const url::Origin& origin,
         const SchemefulSite& site) { return origin == rp_origin; },
      rp_origin));
  {
    base::HistogramTester post_clear_histogram_tester;
    EXPECT_TRUE(manager().AddPreProvisionedKey(
        rp_origin, "after_clear", provider_origin,
        unexportable_keys::UnexportableSigningKeyId()));
    post_clear_histogram_tester.ExpectUniqueSample(
        "Net.DeviceBoundSessions.PreProvisionedKeyCount", 2, 1);
  }
}

TEST_F(SingleSignOnKeyManagerTest, AddPreProvisionedKey_SameKeyForTwoRps) {
  auto rp1_origin = url::Origin::Create(GURL("https://rp1.test"));
  auto rp2_origin = url::Origin::Create(GURL("https://rp2.test"));
  auto provider_origin = ProviderOrigin();
  unexportable_keys::UnexportableSigningKeyId key1_id;
  unexportable_keys::UnexportableSigningKeyId key2_id;

  EXPECT_TRUE(manager().AddPreProvisionedKey(rp1_origin, "key", provider_origin,
                                             key1_id));
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(manager().AddPreProvisionedKey(rp2_origin, "key", provider_origin,
                                             key2_id));
  // The count covers the keys of all RPs.
  histogram_tester.ExpectUniqueSample(
      "Net.DeviceBoundSessions.PreProvisionedKeyCount", 2, 1);

  EXPECT_THAT(manager().FindPreProvisionedKey("key", rp1_origin),
              ValueIs(key1_id));
  EXPECT_THAT(manager().FindPreProvisionedKey("key", rp2_origin),
              ValueIs(key2_id));
}

TEST_F(SingleSignOnKeyManagerTest,
       ClearPreProvisionedKeys_NullMatcher_ClearsAllKeys) {
  auto rp1_origin = url::Origin::Create(GURL("https://rp1.test"));
  auto rp2_origin = url::Origin::Create(GURL("https://rp2.test"));
  auto provider_origin = ProviderOrigin();
  unexportable_keys::UnexportableSigningKeyId key1_id;
  unexportable_keys::UnexportableSigningKeyId key2_id;

  EXPECT_TRUE(manager().AddPreProvisionedKey(rp1_origin, "key1",
                                             provider_origin, key1_id));
  EXPECT_TRUE(manager().AddPreProvisionedKey(rp2_origin, "key2",
                                             provider_origin, key2_id));

  EXPECT_CALL(mock_key_service(),
              DeleteKeysSlowlyAsync(
                  UnorderedElementsAre(key1_id, key2_id),
                  unexportable_keys::BackgroundTaskPriority::kBestEffort, _));

  manager().ClearPreProvisionedKeys(base::NullCallback());

  EXPECT_THAT(manager().FindPreProvisionedKey("key1", rp1_origin),
              ErrorIs(SessionError::kPreProvisionedKeyNotFound));
  EXPECT_THAT(manager().FindPreProvisionedKey("key2", rp2_origin),
              ErrorIs(SessionError::kPreProvisionedKeyNotFound));
}

TEST_F(SingleSignOnKeyManagerTest,
       ClearPreProvisionedKeys_Matcher_ClearsMatchingRpKeysOnly) {
  auto rp1_origin = url::Origin::Create(GURL("https://rp1.test"));
  auto rp2_origin = url::Origin::Create(GURL("https://rp2.test"));
  auto provider_origin = ProviderOrigin();
  unexportable_keys::UnexportableSigningKeyId key1a_id;
  unexportable_keys::UnexportableSigningKeyId key1b_id;
  unexportable_keys::UnexportableSigningKeyId key2_id;

  EXPECT_TRUE(manager().AddPreProvisionedKey(rp1_origin, "key1a",
                                             provider_origin, key1a_id));
  EXPECT_TRUE(manager().AddPreProvisionedKey(rp1_origin, "key1b",
                                             provider_origin, key1b_id));
  EXPECT_TRUE(manager().AddPreProvisionedKey(rp2_origin, "key2",
                                             provider_origin, key2_id));

  EXPECT_CALL(mock_key_service(),
              DeleteKeysSlowlyAsync(
                  UnorderedElementsAre(key1a_id, key1b_id),
                  unexportable_keys::BackgroundTaskPriority::kBestEffort, _));

  manager().ClearPreProvisionedKeys(base::BindRepeating(
      [](const url::Origin& origin, const SchemefulSite& site) {
        return origin.host() == "rp1.test";
      }));

  EXPECT_THAT(manager().FindPreProvisionedKey("key1a", rp1_origin),
              ErrorIs(SessionError::kPreProvisionedKeyNotFound));
  EXPECT_THAT(manager().FindPreProvisionedKey("key1b", rp1_origin),
              ErrorIs(SessionError::kPreProvisionedKeyNotFound));
  EXPECT_THAT(manager().FindPreProvisionedKey("key2", rp2_origin),
              ValueIs(key2_id));
}

TEST_F(SingleSignOnKeyManagerTest,
       ClearPreProvisionedKeys_IdpMatcher_KeepsRpKeys) {
  auto rp_origin = url::Origin::Create(GURL("https://rp.test"));
  auto provider_origin = ProviderOrigin();
  unexportable_keys::UnexportableSigningKeyId key_id;

  EXPECT_TRUE(manager().AddPreProvisionedKey(rp_origin, "key1", provider_origin,
                                             key_id));

  EXPECT_CALL(mock_key_service(), DeleteKeysSlowlyAsync).Times(0);

  manager().ClearPreProvisionedKeys(base::BindRepeating(
      [](const url::Origin& origin, const SchemefulSite& site) {
        return origin == ProviderOrigin();
      }));

  EXPECT_THAT(manager().FindPreProvisionedKey("key1", rp_origin),
              ValueIs(key_id));
}

TEST_F(SingleSignOnKeyManagerTest, DestructorDeletesAllStoredKeys) {
  auto rp1_origin = url::Origin::Create(GURL("https://rp1.test"));
  auto rp2_origin = url::Origin::Create(GURL("https://rp2.test"));
  auto provider_origin = ProviderOrigin();
  unexportable_keys::UnexportableSigningKeyId key1_id;
  unexportable_keys::UnexportableSigningKeyId key2_id;

  EXPECT_TRUE(manager().AddPreProvisionedKey(rp1_origin, "key1",
                                             provider_origin, key1_id));
  EXPECT_TRUE(manager().AddPreProvisionedKey(rp2_origin, "key2",
                                             provider_origin, key2_id));

  EXPECT_CALL(mock_key_service(),
              DeleteKeysSlowlyAsync(
                  UnorderedElementsAre(key1_id, key2_id),
                  unexportable_keys::BackgroundTaskPriority::kBestEffort, _));

  reset_manager();
}

TEST_F(SingleSignOnKeyManagerTest,
       DestructorWithNoStoredKeysDoesNotDeleteKeys) {
  EXPECT_CALL(mock_key_service(), DeleteKeysSlowlyAsync).Times(0);

  reset_manager();
}

TEST_F(SingleSignOnKeyManagerTest, ConsumePreProvisionedKeyRemovesMatchingKey) {
  auto rp_origin = url::Origin::Create(GURL("https://rp.test"));
  auto provider_origin = ProviderOrigin();
  unexportable_keys::UnexportableSigningKeyId key1_id;
  unexportable_keys::UnexportableSigningKeyId key2_id;

  EXPECT_TRUE(manager().AddPreProvisionedKey(rp_origin, "key1", provider_origin,
                                             key1_id));
  EXPECT_TRUE(manager().AddPreProvisionedKey(rp_origin, "key2", provider_origin,
                                             key2_id));

  EXPECT_TRUE(manager().ConsumePreProvisionedKey(key1_id));

  EXPECT_THAT(manager().FindPreProvisionedKey("key1", rp_origin),
              ErrorIs(SessionError::kPreProvisionedKeyNotFound));
  EXPECT_THAT(manager().FindPreProvisionedKey("key2", rp_origin),
              ValueIs(key2_id));

  // The claimed key must not be deleted when the manager is destroyed.
  EXPECT_CALL(mock_key_service(),
              DeleteKeysSlowlyAsync(
                  ElementsAre(key2_id),
                  unexportable_keys::BackgroundTaskPriority::kBestEffort, _));
  reset_manager();
}

TEST_F(SingleSignOnKeyManagerTest,
       ConsumePreProvisionedKeyUnknownKeyIdReturnsFalse) {
  auto rp_origin = url::Origin::Create(GURL("https://rp.test"));
  auto provider_origin = ProviderOrigin();
  unexportable_keys::UnexportableSigningKeyId key_id;

  EXPECT_TRUE(manager().AddPreProvisionedKey(rp_origin, "key1", provider_origin,
                                             key_id));

  EXPECT_FALSE(manager().ConsumePreProvisionedKey(
      unexportable_keys::UnexportableSigningKeyId()));

  EXPECT_THAT(manager().FindPreProvisionedKey("key1", rp_origin),
              ValueIs(key_id));
}

TEST_F(SingleSignOnKeyManagerTest, ConsumePreProvisionedKeyKeepsOtherKeys) {
  auto rp1_origin = url::Origin::Create(GURL("https://rp1.test"));
  auto rp2_origin = url::Origin::Create(GURL("https://rp2.test"));
  auto provider_origin = ProviderOrigin();
  unexportable_keys::UnexportableSigningKeyId key1a_id;
  unexportable_keys::UnexportableSigningKeyId key1b_id;
  unexportable_keys::UnexportableSigningKeyId key2_id;

  EXPECT_TRUE(manager().AddPreProvisionedKey(rp1_origin, "key1a",
                                             provider_origin, key1a_id));
  EXPECT_TRUE(manager().AddPreProvisionedKey(rp1_origin, "key1b",
                                             provider_origin, key1b_id));
  EXPECT_TRUE(manager().AddPreProvisionedKey(rp2_origin, "key2",
                                             provider_origin, key2_id));

  EXPECT_TRUE(manager().ConsumePreProvisionedKey(key1a_id));

  EXPECT_THAT(manager().FindPreProvisionedKey("key1a", rp1_origin),
              ErrorIs(SessionError::kPreProvisionedKeyNotFound));
  // Other keys of the same RP and keys of other RPs stay.
  EXPECT_THAT(manager().FindPreProvisionedKey("key1b", rp1_origin),
              ValueIs(key1b_id));
  EXPECT_THAT(manager().FindPreProvisionedKey("key2", rp2_origin),
              ValueIs(key2_id));
}

TEST_F(SingleSignOnKeyManagerTest,
       ConsumePreProvisionedKeyAfterClearReturnsFalse) {
  auto rp_origin = url::Origin::Create(GURL("https://rp.test"));
  auto provider_origin = ProviderOrigin();
  unexportable_keys::UnexportableSigningKeyId key_id;

  EXPECT_TRUE(manager().AddPreProvisionedKey(rp_origin, "key1", provider_origin,
                                             key_id));

  EXPECT_CALL(mock_key_service(),
              DeleteKeysSlowlyAsync(
                  ElementsAre(key_id),
                  unexportable_keys::BackgroundTaskPriority::kBestEffort, _));
  manager().ClearPreProvisionedKeys(base::BindRepeating(
      [](const url::Origin& origin, const SchemefulSite& site) {
        return origin.host() == "rp.test";
      }));

  EXPECT_FALSE(manager().ConsumePreProvisionedKey(key_id));
}

TEST_F(SingleSignOnKeyManagerTest,
       AddPreProvisionedKey_DuplicateKey_ReturnsFalse) {
  auto rp_origin = url::Origin::Create(GURL("https://rp.test"));
  auto provider_origin = ProviderOrigin();
  unexportable_keys::UnexportableSigningKeyId key1_id;
  unexportable_keys::UnexportableSigningKeyId key2_id;

  EXPECT_TRUE(manager().AddPreProvisionedKey(rp_origin, "key1", provider_origin,
                                             key1_id));
  EXPECT_FALSE(manager().AddPreProvisionedKey(rp_origin, "key1",
                                              provider_origin, key2_id));
  // The rejected key doesn't replace the stored one.
  EXPECT_THAT(manager().FindPreProvisionedKey("key1", rp_origin),
              ValueIs(key1_id));
}

TEST_F(SingleSignOnKeyManagerTest,
       AddPreProvisionedKeyRejectsSameDigestFromOtherIdp) {
  auto rp_origin = url::Origin::Create(GURL("https://rp.test"));
  unexportable_keys::UnexportableSigningKeyId key_id;

  EXPECT_TRUE(manager().AddPreProvisionedKey(rp_origin, "key1",
                                             ProviderOrigin(), key_id));
  // `FindPreProvisionedKey()` can't tell IdPs apart, so a second IdP can't
  // store the same digest for the same RP.
  EXPECT_FALSE(manager().AddPreProvisionedKey(
      rp_origin, "key1", url::Origin::Create(GURL("https://other-idp.test")),
      unexportable_keys::UnexportableSigningKeyId()));

  // The first IdP keeps its key, and with it the cookie access check.
  SetCookieAccessOnlyFor(ProviderOrigin());
  EXPECT_THAT(manager().FindPreProvisionedKey("key1", rp_origin),
              ValueIs(key_id));
}

using SingleSignOnKeyManagerCanGenerateKeyTest = SingleSignOnKeyManagerTest;
using SingleSignOnKeyManagerGenerateKeyTest = SingleSignOnKeyManagerTest;

TEST_F(SingleSignOnKeyManagerCanGenerateKeyTest, NoCookieAccessReturnsFalse) {
  SetCookieAccess(false);

  EXPECT_FALSE(manager().CanGenerateKey(
      ProviderOrigin(), url::Origin::Create(GURL("https://rp.test"))));
}

TEST_F(SingleSignOnKeyManagerGenerateKeyTest, ValidParamStoresKey) {
  // The generated key must NOT be deleted upon success.
  EXPECT_CALL(mock_key_service(), DeleteKeysSlowlyAsync).Times(0);
  TestFuture<CapturedRpKey> generated;
  ExpectRpKeyGenerated(generated);
  // The request certifies the RP key with the IdP's AIK over the challenge
  // from the param. The call still goes to the real key service.
  const std::string challenge = CreateGenerateKeyParam().challenge;
  EXPECT_CALL(mock_key_service(),
              CertifySlowlyAsync(
                  aik_id(), _, ElementsAreArray(base::as_byte_span(challenge)),
                  unexportable_keys::BackgroundTaskPriority::kUserBlocking, _));

  TestFuture<SessionErrorOr<std::string>> future;
  manager().GenerateKey(aik_id(), CreateGenerateKeyParam(), ProviderOrigin(),
                        future.GetCallback());

  ASSERT_OK_AND_ASSIGN(std::string serialized_statement, future.Take());
  ASSERT_OK_AND_ASSIGN(
      base::DictValue statement_dict,
      base::Base64UrlDecode(serialized_statement,
                            base::Base64UrlDecodePolicy::DISALLOW_PADDING)
          .and_then([](const std::vector<uint8_t>& json) {
            return base::JSONReader::ReadDict(base::as_string_view(json),
                                              base::JSON_PARSE_RFC);
          }));
  // `sig` is an ECDSA signature, which is randomized, so the statement cannot
  // be compared against a fixed dict. Check its exact set of fields instead.
  // Unlike the registration statement, the binding statement omits "fmt".
  EXPECT_FALSE(statement_dict.contains("fmt"));
  EXPECT_TRUE(statement_dict.FindString("stmt"));
  EXPECT_TRUE(statement_dict.FindString("sig"));
  EXPECT_TRUE(statement_dict.FindString("sub_key"));
  EXPECT_THAT(statement_dict, SizeIs(3));

  const CapturedRpKey& key = generated.Get();
  EXPECT_THAT(FindGeneratedKey(key), ValueIs(key.id));
  // The key service still has the stored key.
  EXPECT_THAT(key_service().GetAlgorithm(key.id),
              ValueIs(crypto::sign::ECDSA_SHA256));
}

TEST_F(SingleSignOnKeyManagerGenerateKeyTest, NoSupportedAlgosReturnsError) {
  GenerateKeyParam param = CreateGenerateKeyParam();
  param.supported_algos.clear();

  // Keygen fails before creating a key, so there is nothing to delete.
  EXPECT_CALL(mock_key_service(), DeleteKeysSlowlyAsync).Times(0);

  TestFuture<SessionErrorOr<std::string>> future;
  manager().GenerateKey(aik_id(), std::move(param), ProviderOrigin(),
                        future.GetCallback());
  // The result is always posted, even if the key service replies inline.
  EXPECT_FALSE(future.IsReady());

  EXPECT_THAT(future.Take(), ErrorIs(SessionError::kSigningKeyGenerationError));
}

TEST_F(SingleSignOnKeyManagerGenerateKeyTest,
       CertifyFailsDeletesKeyAndReturnsError) {
  // Delete the AIK so `CertifySlowlyAsync()` fails.
  key_service().DeleteKeysSlowlyAsync(
      {aik_id()}, unexportable_keys::BackgroundTaskPriority::kBestEffort,
      base::DoNothing());
  TestFuture<CapturedRpKey> generated;
  ExpectRpKeyGenerated(generated);
  TestFuture<void> deleted;
  ExpectRpKeyDeleted(deleted);

  TestFuture<SessionErrorOr<std::string>> future;
  manager().GenerateKey(aik_id(), CreateGenerateKeyParam(), ProviderOrigin(),
                        future.GetCallback());
  // `CertifySlowlyAsync()` replies inline for an unknown AIK, but the result
  // is still posted.
  EXPECT_FALSE(future.IsReady());

  EXPECT_THAT(future.Take(),
              ErrorIs(SessionError::kAttestationCertificationError));
  EXPECT_TRUE(deleted.Wait());
  EXPECT_THAT(FindGeneratedKey(generated.Get()),
              ErrorIs(SessionError::kPreProvisionedKeyNotFound));
}

TEST_F(SingleSignOnKeyManagerGenerateKeyTest,
       ThumbprintFailsDeletesKeyAndReturnsError) {
  // The key service has no public key for an unknown key ID, so computing the
  // thumbprint fails.
  EXPECT_CALL(mock_key_service(), GenerateSigningKeySlowlyAsync)
      .WillOnce(base::test::RunOnceCallback<2>(
          unexportable_keys::UnexportableSigningKeyId()));
  // The request stops before the slow certify call.
  EXPECT_CALL(mock_key_service(), CertifySlowlyAsync).Times(0);
  // The request still deletes the key it got from keygen.
  TestFuture<void> deleted;
  ExpectRpKeyDeleted(deleted);

  TestFuture<SessionErrorOr<std::string>> future;
  manager().GenerateKey(aik_id(), CreateGenerateKeyParam(), ProviderOrigin(),
                        future.GetCallback());

  EXPECT_THAT(future.Take(), ErrorIs(SessionError::kSigningKeyGenerationError));
  EXPECT_TRUE(deleted.Wait());
}

TEST_F(SingleSignOnKeyManagerGenerateKeyTest, CancelledDuringKeygenDeletesKey) {
  base::CancelableOnceCallback<void(SessionErrorOr<std::string>)> key_callback(
      base::DoNothing());

  // The generated RP key must be deleted upon cancellation.
  TestFuture<CapturedRpKey> generated;
  ExpectRpKeyGenerated(generated);
  TestFuture<void> delete_future;
  ExpectRpKeyDeleted(delete_future);
  // Certify must not be invoked since keygen continuation observed
  // cancellation.
  EXPECT_CALL(mock_key_service(), CertifySlowlyAsync).Times(0);

  manager().GenerateKey(aik_id(), CreateGenerateKeyParam(), ProviderOrigin(),
                        key_callback.callback());
  // Cancel before keygen replies.
  key_callback.Cancel();

  EXPECT_TRUE(delete_future.Wait());
  EXPECT_THAT(key_service().GetAlgorithm(generated.Get().id),
              ErrorIs(unexportable_keys::ServiceError::kKeyNotFound));
  EXPECT_THAT(FindGeneratedKey(generated.Get()),
              ErrorIs(SessionError::kPreProvisionedKeyNotFound));
}

TEST_F(SingleSignOnKeyManagerGenerateKeyTest,
       CancelledDuringCertifyDeletesKey) {
  base::CancelableOnceCallback<void(SessionErrorOr<std::string>)> key_callback(
      base::DoNothing());

  // The generated RP key must be deleted upon cancellation during certify.
  TestFuture<CapturedRpKey> generated;
  ExpectRpKeyGenerated(generated);
  TestFuture<base::OnceClosure> certify;
  HoldCertify(certify);
  TestFuture<void> delete_future;
  ExpectRpKeyDeleted(delete_future);

  manager().GenerateKey(aik_id(), CreateGenerateKeyParam(), ProviderOrigin(),
                        key_callback.callback());
  base::OnceClosure release_certify = certify.Take();
  key_callback.Cancel();
  std::move(release_certify).Run();

  EXPECT_TRUE(delete_future.Wait());
  EXPECT_THAT(key_service().GetAlgorithm(generated.Get().id),
              ErrorIs(unexportable_keys::ServiceError::kKeyNotFound));
  EXPECT_THAT(FindGeneratedKey(generated.Get()),
              ErrorIs(SessionError::kPreProvisionedKeyNotFound));
}

TEST_F(SingleSignOnKeyManagerGenerateKeyTest,
       SiteDataClearedDeletesKeyAndReturnsError) {
  TestFuture<CapturedRpKey> generated;
  ExpectRpKeyGenerated(generated);
  // The request stops before the slow certify call.
  EXPECT_CALL(mock_key_service(), CertifySlowlyAsync).Times(0);
  // The generated RP key must not be stored after the RP's data was cleared.
  TestFuture<void> deleted;
  ExpectRpKeyDeleted(deleted);

  TestFuture<SessionErrorOr<std::string>> future;
  manager().GenerateKey(aik_id(), CreateGenerateKeyParam(), ProviderOrigin(),
                        future.GetCallback());
  manager().ClearPreProvisionedKeys(base::BindRepeating(
      [](const url::Origin& origin, const SchemefulSite& site) {
        return origin.host() == "rp.test";
      }));

  EXPECT_THAT(future.Take(), ErrorIs(SessionError::kSigningKeyGenerationError));
  EXPECT_TRUE(deleted.Wait());
  EXPECT_THAT(FindGeneratedKey(generated.Get()),
              ErrorIs(SessionError::kPreProvisionedKeyNotFound));
}

TEST_F(SingleSignOnKeyManagerGenerateKeyTest,
       AllSiteDataClearedDeletesKeyAndReturnsError) {
  TestFuture<CapturedRpKey> generated;
  ExpectRpKeyGenerated(generated);
  TestFuture<void> deleted;
  ExpectRpKeyDeleted(deleted);

  TestFuture<SessionErrorOr<std::string>> future;
  manager().GenerateKey(aik_id(), CreateGenerateKeyParam(), ProviderOrigin(),
                        future.GetCallback());
  // A null matcher clears everything, including in-flight requests.
  manager().ClearPreProvisionedKeys(base::NullCallback());

  EXPECT_THAT(future.Take(), ErrorIs(SessionError::kSigningKeyGenerationError));
  EXPECT_TRUE(deleted.Wait());
  EXPECT_THAT(FindGeneratedKey(generated.Get()),
              ErrorIs(SessionError::kPreProvisionedKeyNotFound));
}

TEST_F(SingleSignOnKeyManagerGenerateKeyTest, OtherSiteDataClearedStoresKey) {
  TestFuture<CapturedRpKey> generated;
  ExpectRpKeyGenerated(generated);

  TestFuture<SessionErrorOr<std::string>> future;
  manager().GenerateKey(aik_id(), CreateGenerateKeyParam(), ProviderOrigin(),
                        future.GetCallback());
  manager().ClearPreProvisionedKeys(base::BindRepeating(
      [](const url::Origin& origin, const SchemefulSite& site) {
        return origin.host() == "other-rp.test";
      }));

  EXPECT_THAT(future.Take(), HasValue());
  EXPECT_THAT(FindGeneratedKey(generated.Get()), ValueIs(generated.Get().id));
}

TEST_F(SingleSignOnKeyManagerGenerateKeyTest,
       CookieAccessRevokedDeletesKeyAndReturnsError) {
  TestFuture<CapturedRpKey> generated;
  ExpectRpKeyGenerated(generated);
  TestFuture<void> deleted;
  ExpectRpKeyDeleted(deleted);

  TestFuture<SessionErrorOr<std::string>> future;
  manager().GenerateKey(aik_id(), CreateGenerateKeyParam(), ProviderOrigin(),
                        future.GetCallback());
  // Revoke access while the request runs, so storing the key fails.
  SetCookieAccess(false);

  EXPECT_THAT(future.Take(), ErrorIs(SessionError::kSigningKeyGenerationError));
  EXPECT_TRUE(deleted.Wait());
  EXPECT_THAT(key_service().GetAlgorithm(generated.Get().id),
              ErrorIs(unexportable_keys::ServiceError::kKeyNotFound));
  SetCookieAccess(true);
  EXPECT_THAT(FindGeneratedKey(generated.Get()),
              ErrorIs(SessionError::kPreProvisionedKeyNotFound));
}

TEST_F(SingleSignOnKeyManagerGenerateKeyTest,
       ManagerDestroyedDuringKeygenDeletesKey) {
  TestFuture<CapturedRpKey> generated;
  ExpectRpKeyGenerated(generated);
  TestFuture<void> deleted;
  ExpectRpKeyDeleted(deleted);

  TestFuture<SessionErrorOr<std::string>> future;
  manager().GenerateKey(aik_id(), CreateGenerateKeyParam(), ProviderOrigin(),
                        future.GetCallback());

  // Destroy the manager while keygen is pending.
  reset_manager();

  EXPECT_TRUE(deleted.Wait());
  EXPECT_THAT(key_service().GetAlgorithm(generated.Get().id),
              ErrorIs(unexportable_keys::ServiceError::kKeyNotFound));
  EXPECT_FALSE(future.IsReady());
}

TEST_F(SingleSignOnKeyManagerGenerateKeyTest,
       ManagerDestroyedThenKeygenFailsDoesNotDeleteKey) {
  // Hold the keygen callback, so the manager can be destroyed before it runs.
  base::OnceCallback<void(unexportable_keys::ServiceErrorOr<
                          unexportable_keys::UnexportableSigningKeyId>)>
      keygen_callback;
  EXPECT_CALL(mock_key_service(), GenerateSigningKeySlowlyAsync)
      .WillOnce(MoveArg<2>(&keygen_callback));
  // There is no key to delete, and the key service may already be gone.
  EXPECT_CALL(mock_key_service(), DeleteKeysSlowlyAsync).Times(0);

  TestFuture<SessionErrorOr<std::string>> future;
  manager().GenerateKey(aik_id(), CreateGenerateKeyParam(), ProviderOrigin(),
                        future.GetCallback());
  ASSERT_TRUE(keygen_callback);

  reset_manager();
  std::move(keygen_callback)
      .Run(base::unexpected(
          unexportable_keys::ServiceError::kOperationCancelled));

  EXPECT_FALSE(future.IsReady());
}

TEST_F(SingleSignOnKeyManagerGenerateKeyTest,
       ManagerDestroyedDuringCertifyDeletesKey) {
  // Hold the certify call, so the manager can be destroyed while it runs.
  TestFuture<base::OnceClosure> certify;
  HoldCertify(certify);
  TestFuture<void> deleted;
  ExpectRpKeyDeleted(deleted);

  TestFuture<SessionErrorOr<std::string>> future;
  manager().GenerateKey(aik_id(), CreateGenerateKeyParam(), ProviderOrigin(),
                        future.GetCallback());
  ASSERT_TRUE(certify.Wait());

  reset_manager();

  EXPECT_TRUE(deleted.Wait());
  EXPECT_FALSE(future.IsReady());
}

TEST_F(SingleSignOnKeyManagerGenerateKeyTest,
       ManagerDestroyedWhileResultIsPostedDeletesKey) {
  TestFuture<CapturedRpKey> generated;
  ExpectRpKeyGenerated(generated);
  TestFuture<void> deleted;
  ExpectRpKeyDeleted(deleted);
  // Destroy the manager right after certify replied, while the result is
  // posted but has not run yet.
  EXPECT_CALL(mock_key_service(), CertifySlowlyAsync)
      .WillOnce(
          [this](unexportable_keys::UnexportableAttestationKeyId aik_id,
                 unexportable_keys::UnexportableSigningKeyId key_id,
                 base::span<const uint8_t> challenge,
                 unexportable_keys::BackgroundTaskPriority priority,
                 base::OnceCallback<void(unexportable_keys::ServiceErrorOr<
                                         crypto::AttestationStatement>)>
                     callback) {
            key_service().CertifySlowlyAsync(
                aik_id, key_id, challenge, priority,
                base::BindLambdaForTesting(
                    [this, callback = std::move(callback)](
                        unexportable_keys::ServiceErrorOr<
                            crypto::AttestationStatement> result) mutable {
                      std::move(callback).Run(std::move(result));
                      reset_manager();
                    }));
          });

  TestFuture<SessionErrorOr<std::string>> future;
  manager().GenerateKey(aik_id(), CreateGenerateKeyParam(), ProviderOrigin(),
                        future.GetCallback());

  EXPECT_TRUE(deleted.Wait());
  EXPECT_THAT(key_service().GetAlgorithm(generated.Get().id),
              ErrorIs(unexportable_keys::ServiceError::kKeyNotFound));
  EXPECT_FALSE(future.IsReady());
}

TEST_F(SingleSignOnKeyManagerGenerateKeyTest,
       SiteDataClearedDuringCertifyDeletesKeyAndReturnsError) {
  TestFuture<CapturedRpKey> generated;
  ExpectRpKeyGenerated(generated);
  TestFuture<base::OnceClosure> certify;
  HoldCertify(certify);
  TestFuture<void> deleted;
  ExpectRpKeyDeleted(deleted);

  TestFuture<SessionErrorOr<std::string>> future;
  manager().GenerateKey(aik_id(), CreateGenerateKeyParam(), ProviderOrigin(),
                        future.GetCallback());
  base::OnceClosure release_certify = certify.Take();
  manager().ClearPreProvisionedKeys(base::BindRepeating(
      [](const url::Origin& origin, const SchemefulSite& site) {
        return origin.host() == "rp.test";
      }));
  // Certify succeeds, but the key must not be stored anymore.
  std::move(release_certify).Run();

  EXPECT_THAT(future.Take(), ErrorIs(SessionError::kSigningKeyGenerationError));
  EXPECT_TRUE(deleted.Wait());
  EXPECT_THAT(key_service().GetAlgorithm(generated.Get().id),
              ErrorIs(unexportable_keys::ServiceError::kKeyNotFound));
  EXPECT_THAT(FindGeneratedKey(generated.Get()),
              ErrorIs(SessionError::kPreProvisionedKeyNotFound));
}

// `OnGenerateKeyDone()` runs the callback last, so it may destroy the manager
// or start a new request.
TEST_F(SingleSignOnKeyManagerGenerateKeyTest,
       ManagerDestroyedInCallbackDoesNotCrash) {
  TestFuture<void> callback_ran;
  manager().GenerateKey(
      aik_id(), CreateGenerateKeyParam(), ProviderOrigin(),
      base::BindLambdaForTesting([&](SessionErrorOr<std::string> result) {
        // Synchronously destroy manager from within the callback.
        reset_manager();
        callback_ran.SetValue();
      }));

  EXPECT_TRUE(callback_ran.Wait());
}

TEST_F(SingleSignOnKeyManagerGenerateKeyTest, NewRequestInCallbackSucceeds) {
  TestFuture<SessionErrorOr<std::string>> future2;
  bool first_callback_ran = false;
  manager().GenerateKey(
      aik_id(), CreateGenerateKeyParam(), ProviderOrigin(),
      base::BindLambdaForTesting([&](SessionErrorOr<std::string> result) {
        EXPECT_THAT(result, HasValue());
        first_callback_ran = true;
        // Start a second request synchronously from within the first callback.
        manager().GenerateKey(aik_id(), CreateGenerateKeyParam(),
                              ProviderOrigin(), future2.GetCallback());
      }));

  ASSERT_OK_AND_ASSIGN(std::string statement2, future2.Take());
  EXPECT_TRUE(first_callback_ran);
  EXPECT_FALSE(statement2.empty());
}

}  // namespace

}  // namespace net::device_bound_sessions
