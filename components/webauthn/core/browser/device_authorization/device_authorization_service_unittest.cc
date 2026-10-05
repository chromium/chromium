// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/version_info/channel.h"
#include "components/webauthn/core/browser/device_authorization/device_authorization_client.h"
#include "components/webauthn/core/browser/device_authorization/device_authorization_service_impl.h"
#include "components/webauthn/core/browser/device_authorization/device_authorization_switches.h"
#include "components/webauthn/core/browser/test_passkey_model.h"
#include "google_apis/gaia/gaia_id.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace webauthn {
namespace {

using ::base::test::TestFuture;
using ::testing::SizeIs;

constexpr char kTestEmail[] = "test@example.com";
#if !BUILDFLAG(IS_CHROMEOS)
constexpr char kOtherEmail[] = "other@example.com";
#endif  // !BUILDFLAG(IS_CHROMEOS)
constexpr char kKeyBytes[] = "fake_device_auth_key";
constexpr int32_t kKeyProtoVersion = 1;
constexpr int32_t kNewerKeyProtoVersion = 2;
constexpr char kFakeWebFallbackUrl[] = "https://example.com/reauth";
constexpr char kCustomRapt[] = "custom_rapt_token";
constexpr char kRpId[] = "example.com";
constexpr char kEncryptedPasskeyData[] = "encrypted_passkey_data";

sync_pb::GetDeviceAuthorizationKeyResponse CreateSuccessResponse() {
  sync_pb::GetDeviceAuthorizationKeyResponse response;
  auto* keys = response.mutable_device_authorization_keys();
  auto* key = keys->add_keys();
  key->set_version(kKeyProtoVersion);
  key->set_key(kKeyBytes);
  return response;
}

sync_pb::GetDeviceAuthorizationKeyResponse CreateReAuthResponse() {
  sync_pb::GetDeviceAuthorizationKeyResponse response;
  auto* reauth = response.mutable_re_auth_params();
  reauth->set_plt("fake_plt");
  reauth->set_web_fallback_url(kFakeWebFallbackUrl);
  return response;
}

CachedDeviceAuthorizationKeys CreateCachedKeys(int cache_version,
                                               const std::string& key_bytes) {
  CachedDeviceAuthorizationKeys cached_keys;
  cached_keys.set_cache_version(cache_version);
  DeviceAuthorizationKey* key = cached_keys.mutable_keys()->add_keys();
  key->set_version(kKeyProtoVersion);
  key->set_key(key_bytes);
  return cached_keys;
}

class TestDeviceAuthorizationClient : public DeviceAuthorizationClient {
 public:
  TestDeviceAuthorizationClient() = default;
  ~TestDeviceAuthorizationClient() override = default;

  void GetCachedKeys(const GaiaId& gaia_id,
                     GetCachedKeysCallback callback) override {
    auto it = storage_.find(gaia_id);
    if (it == storage_.end()) {
      std::move(callback).Run(std::nullopt);
      return;
    }
    std::move(callback).Run(it->second);
  }

  void StoreKeys(const GaiaId& gaia_id,
                 const CachedDeviceAuthorizationKeys& keys,
                 StoreKeysCallback callback) override {
    storage_[gaia_id] = keys;
    std::move(callback).Run(true);
  }

  void PopulatePlatformData(const GaiaId& gaia_id,
                            sync_pb::GetDeviceAuthorizationKeyRequest request,
                            PopulatePlatformDataCallback callback) override {
    request.MergeFrom(request_);
    std::move(callback).Run(std::move(request));
  }

  void set_request(sync_pb::GetDeviceAuthorizationKeyRequest request) {
    request_ = std::move(request);
  }

 private:
  std::map<GaiaId, CachedDeviceAuthorizationKeys> storage_;
  sync_pb::GetDeviceAuthorizationKeyRequest request_;
};

}  // namespace

class DeviceAuthorizationServiceImplTest : public testing::Test {
 protected:
  DeviceAuthorizationServiceImplTest()
      : shared_url_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  void SetUp() override {
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
    auto client = std::make_unique<TestDeviceAuthorizationClient>();
    client_ = client.get();
    service_ = std::make_unique<DeviceAuthorizationServiceImpl>(
        identity_test_env_.identity_manager(), &passkey_model_,
        shared_url_loader_factory_, std::move(client),
        version_info::Channel::UNKNOWN);
  }

  void TearDown() override {
    if (service_) {
      DestroyService();
    }
  }

  void DestroyService() {
    client_ = nullptr;
    service_->Shutdown();
    service_.reset();
  }

  GaiaId SignInPrimaryAccount(std::string_view email = kTestEmail) {
    identity_test_env_.MakePrimaryAccountAvailable(
        email, signin::ConsentLevel::kSignin);
    return identity_test_env_.identity_manager()
        ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
        .gaia;
  }

  void SetResponseForEndpoint(
      const sync_pb::GetDeviceAuthorizationKeyResponse& response,
      net::HttpStatusCode status = net::HTTP_OK) {
    test_url_loader_factory_.AddResponse(kDeviceAuthorizationKeyEndpointUrl,
                                         response.SerializeAsString(), status);
  }

  bool HasCachedKeys(const GaiaId& gaia_id) {
    TestFuture<std::optional<CachedDeviceAuthorizationKeys>> future;
    client_->GetCachedKeys(gaia_id, future.GetCallback());
    return future.Get().has_value();
  }

  void StoreCachedKeys(const GaiaId& gaia_id,
                       const CachedDeviceAuthorizationKeys& keys) {
    TestFuture<bool> future;
    client_->StoreKeys(gaia_id, keys, future.GetCallback());
    ASSERT_TRUE(future.Get());
  }

  // Adds a passkey encrypted with the device authorization key `key_version`.
  void AddPasskeyEncryptedWithKeyVersion(int32_t key_version) {
    sync_pb::WebauthnCredentialSpecifics passkey;
    passkey.set_sync_id(base::NumberToString(key_version));
    passkey.set_credential_id(base::NumberToString(key_version));
    passkey.set_rp_id(kRpId);
    passkey.set_security_domain_encrypted(kEncryptedPasskeyData);
    passkey.set_device_authorization_key_version(key_version);
    passkey_model_.AddNewPasskeyForTesting(std::move(passkey));
  }

  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  TestPasskeyModel passkey_model_;
  raw_ptr<TestDeviceAuthorizationClient> client_ = nullptr;
  std::unique_ptr<DeviceAuthorizationServiceImpl> service_;
};

// Test that when keys are already cached, GetOrFetchKeys returns them
// immediately without network fetch.
TEST_F(DeviceAuthorizationServiceImplTest,
       TestGetCachedKeysImmediatelyReturned) {
  GaiaId gaia_id = SignInPrimaryAccount();

  TestFuture<bool> store_future;
  client_->StoreKeys(gaia_id, CreateCachedKeys(1, kKeyBytes),
                     store_future.GetCallback());
  ASSERT_TRUE(store_future.Get());

  TestFuture<DeviceAuthFetchResult> future;
  service_->GetOrFetchKeys(future.GetCallback());

  const DeviceAuthFetchResult& result = future.Get();
  EXPECT_EQ(result.status(), DeviceAuthFetchResult::Status::kSuccess);
  ASSERT_TRUE(result.keys());
  EXPECT_THAT(result.keys()->keys(), SizeIs(1));
  EXPECT_EQ(result.keys()->keys(0).key(), kKeyBytes);
}

// Test that when cached keys have a mismatched cache version, they are
// invalidated/cleared and re-fetched from the server.
TEST_F(DeviceAuthorizationServiceImplTest,
       TestCacheInvalidationOnVersionMismatch) {
  GaiaId gaia_id = SignInPrimaryAccount();
  SetResponseForEndpoint(CreateSuccessResponse());

  TestFuture<bool> store_future;
  // Store keys with an outdated cache version (0 != 1).
  client_->StoreKeys(gaia_id, CreateCachedKeys(0, "stale_key_bytes"),
                     store_future.GetCallback());
  ASSERT_TRUE(store_future.Get());

  TestFuture<DeviceAuthFetchResult> future;
  service_->GetOrFetchKeys(future.GetCallback());

  const DeviceAuthFetchResult& result = future.Get();
  EXPECT_EQ(result.status(), DeviceAuthFetchResult::Status::kSuccess);
  ASSERT_TRUE(result.keys());
  EXPECT_THAT(result.keys()->keys(), SizeIs(1));
  EXPECT_EQ(result.keys()->keys(0).key(), kKeyBytes);

  // Verify that new keys were stored with the current cache version.
  TestFuture<std::optional<CachedDeviceAuthorizationKeys>> stored_future;
  client_->GetCachedKeys(gaia_id, stored_future.GetCallback());
  const std::optional<CachedDeviceAuthorizationKeys>& stored =
      stored_future.Get();
  ASSERT_TRUE(stored.has_value());
  EXPECT_EQ(stored->cache_version(), 1);
  EXPECT_THAT(stored->keys().keys(), SizeIs(1));
  EXPECT_EQ(stored->keys().keys(0).key(), kKeyBytes);
}

// Test that getting or fetching keys fails when no primary account is signed
// in.
TEST_F(DeviceAuthorizationServiceImplTest, TestNotSignedInFails) {
  TestFuture<DeviceAuthFetchResult> future;
  service_->GetOrFetchKeys(future.GetCallback());

  ASSERT_TRUE(future.IsReady());
  const DeviceAuthFetchResult& result = future.Get();
  EXPECT_EQ(result.status(), DeviceAuthFetchResult::Status::kError);
  EXPECT_FALSE(result.keys());
  EXPECT_FALSE(result.reauth_params());
}

// Test that a cache miss initiates a network request, successfully receives
// keys, persists them to storage, and returns them to the caller.
TEST_F(DeviceAuthorizationServiceImplTest,
       TestGetOrFetchKeysSuccessAndPersist) {
  GaiaId gaia_id = SignInPrimaryAccount();
  SetResponseForEndpoint(CreateSuccessResponse());

  TestFuture<DeviceAuthFetchResult> future;
  service_->GetOrFetchKeys(future.GetCallback());

  const DeviceAuthFetchResult& result = future.Get();
  EXPECT_EQ(result.status(), DeviceAuthFetchResult::Status::kSuccess);
  ASSERT_TRUE(result.keys());
  EXPECT_THAT(result.keys()->keys(), SizeIs(1));
  EXPECT_EQ(result.keys()->keys(0).key(), kKeyBytes);

  TestFuture<std::optional<CachedDeviceAuthorizationKeys>> stored_future;
  client_->GetCachedKeys(gaia_id, stored_future.GetCallback());
  const std::optional<CachedDeviceAuthorizationKeys>& stored =
      stored_future.Get();
  ASSERT_TRUE(stored.has_value());
  EXPECT_EQ(stored->cache_version(), 1);
  EXPECT_THAT(stored->keys().keys(), SizeIs(1));
  EXPECT_EQ(stored->keys().keys(0).key(), kKeyBytes);
}

// Test that concurrent calls to `GetOrFetchKeys` for the same GaiaId are
// coalesced into a single network request, and all callers receive the result.
TEST_F(DeviceAuthorizationServiceImplTest,
       TestGetOrFetchKeysConcurrentCallsCoalesced) {
  SignInPrimaryAccount();
  SetResponseForEndpoint(CreateSuccessResponse());

  TestFuture<DeviceAuthFetchResult> future1;
  TestFuture<DeviceAuthFetchResult> future2;

  service_->GetOrFetchKeys(future1.GetCallback());
  service_->GetOrFetchKeys(future2.GetCallback());

  // Both should still be waiting on the single in-flight network request.
  EXPECT_FALSE(future1.IsReady());
  EXPECT_FALSE(future2.IsReady());

  // First call finishes successfully when the response arrives.
  const DeviceAuthFetchResult& result1 = future1.Get();
  EXPECT_EQ(result1.status(), DeviceAuthFetchResult::Status::kSuccess);
  ASSERT_TRUE(result1.keys());
  EXPECT_THAT(result1.keys()->keys(), SizeIs(1));
  EXPECT_EQ(result1.keys()->keys(0).key(), kKeyBytes);

  // Second coalesced call also finishes successfully with identical result.
  const DeviceAuthFetchResult& result2 = future2.Get();
  EXPECT_EQ(result2.status(), DeviceAuthFetchResult::Status::kSuccess);
  ASSERT_TRUE(result2.keys());
  EXPECT_THAT(result2.keys()->keys(), SizeIs(1));
  EXPECT_EQ(result2.keys()->keys(0).key(), kKeyBytes);

  // Only one network request should have been dispatched.
  EXPECT_EQ(test_url_loader_factory_.total_requests(), 1u);
}

// Test that if the server returns `re_auth_params`, the service reports
// `kReAuthRequired` with the parameters populated and does not cache keys.
TEST_F(DeviceAuthorizationServiceImplTest,
       TestGetOrFetchKeysReturnsReAuthRequired) {
  GaiaId gaia_id = SignInPrimaryAccount();
  SetResponseForEndpoint(CreateReAuthResponse());

  TestFuture<DeviceAuthFetchResult> future;
  service_->GetOrFetchKeys(future.GetCallback());

  const DeviceAuthFetchResult& result = future.Get();
  EXPECT_EQ(result.status(), DeviceAuthFetchResult::Status::kReAuthRequired);
  EXPECT_FALSE(result.keys());
  ASSERT_TRUE(result.reauth_params());
  EXPECT_EQ(result.reauth_params()->web_fallback_url(), kFakeWebFallbackUrl);
  TestFuture<std::optional<CachedDeviceAuthorizationKeys>> stored_future;
  client_->GetCachedKeys(gaia_id, stored_future.GetCallback());
  EXPECT_FALSE(stored_future.Get().has_value());
}

// Test that passing `reauth_proof_token` bypasses cached keys, passes the token
// in the request to the network fetcher, and stores the newly fetched keys.
TEST_F(DeviceAuthorizationServiceImplTest,
       TestFetchKeysWithReAuthTokenBypassesCache) {
  GaiaId gaia_id = SignInPrimaryAccount();

  // Cache existing keys first.
  TestFuture<bool> store_future;
  client_->StoreKeys(gaia_id, CreateCachedKeys(1, "old_cached_key"),
                     store_future.GetCallback());
  ASSERT_TRUE(store_future.Get());

  SetResponseForEndpoint(CreateSuccessResponse());

  std::string intercepted_body;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& req) {
        intercepted_body = network::GetUploadData(req);
      }));

  TestFuture<DeviceAuthFetchResult> future;
  service_->FetchKeysWithReAuthToken(kCustomRapt, future.GetCallback());

  const DeviceAuthFetchResult& result = future.Get();
  EXPECT_EQ(result.status(), DeviceAuthFetchResult::Status::kSuccess);
  ASSERT_TRUE(result.keys());
  EXPECT_THAT(result.keys()->keys(), SizeIs(1));
  EXPECT_EQ(result.keys()->keys(0).key(), kKeyBytes);

  ASSERT_EQ(test_url_loader_factory_.total_requests(), 1u);
  sync_pb::GetDeviceAuthorizationKeyRequest sent_request;
  ASSERT_TRUE(sent_request.ParseFromString(intercepted_body));
  EXPECT_EQ(sent_request.reauth_proof_token(), kCustomRapt);

  // Stored keys should be updated to newly fetched key.
  TestFuture<std::optional<CachedDeviceAuthorizationKeys>> stored_future;
  client_->GetCachedKeys(gaia_id, stored_future.GetCallback());
  const std::optional<CachedDeviceAuthorizationKeys>& stored =
      stored_future.Get();
  ASSERT_TRUE(stored.has_value());
  EXPECT_EQ(stored->cache_version(), 1);
  EXPECT_EQ(stored->keys().keys(0).key(), kKeyBytes);
}

// Test that GetOrFetchKeys invokes PopulatePlatformData and forwards the
// client-populated request to the network fetcher.
TEST_F(DeviceAuthorizationServiceImplTest,
       TestGetOrFetchKeysClientPopulatesPlatformData) {
  SignInPrimaryAccount();
  SetResponseForEndpoint(CreateSuccessResponse());

  std::string intercepted_body;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& req) {
        intercepted_body = network::GetUploadData(req);
      }));

  sync_pb::GetDeviceAuthorizationKeyRequest platform_data;
  auto* guard_signals = platform_data.mutable_ios_guard_signals();
  guard_signals->set_signals("test_signals");
  guard_signals->set_salt("test_salt");
  client_->set_request(std::move(platform_data));

  TestFuture<DeviceAuthFetchResult> future;
  service_->GetOrFetchKeys(future.GetCallback());

  const DeviceAuthFetchResult& result = future.Get();
  EXPECT_EQ(result.status(), DeviceAuthFetchResult::Status::kSuccess);

  ASSERT_EQ(test_url_loader_factory_.total_requests(), 1u);
  sync_pb::GetDeviceAuthorizationKeyRequest sent_request;
  ASSERT_TRUE(sent_request.ParseFromString(intercepted_body));
  ASSERT_TRUE(sent_request.has_ios_guard_signals());
  EXPECT_EQ(sent_request.ios_guard_signals().signals(), "test_signals");
  EXPECT_EQ(sent_request.ios_guard_signals().salt(), "test_salt");
}

// Test that server returning an HTTP error returns Status::kError.
TEST_F(DeviceAuthorizationServiceImplTest,
       TestGetOrFetchKeysHttpErrorReturnsError) {
  SignInPrimaryAccount();
  test_url_loader_factory_.AddResponse(kDeviceAuthorizationKeyEndpointUrl, "",
                                       net::HTTP_INTERNAL_SERVER_ERROR);

  TestFuture<DeviceAuthFetchResult> future;
  service_->GetOrFetchKeys(future.GetCallback());

  const DeviceAuthFetchResult& result = future.Get();
  EXPECT_EQ(result.status(), DeviceAuthFetchResult::Status::kError);
}

// Test that a network failure returns Status::kError.
TEST_F(DeviceAuthorizationServiceImplTest,
       TestGetOrFetchKeysNetworkErrorReturnsNetworkError) {
  SignInPrimaryAccount();
  test_url_loader_factory_.AddResponse(
      GURL(kDeviceAuthorizationKeyEndpointUrl),
      network::mojom::URLResponseHead::New(), "",
      network::URLLoaderCompletionStatus(net::ERR_CONNECTION_FAILED));

  TestFuture<DeviceAuthFetchResult> future;
  service_->GetOrFetchKeys(future.GetCallback());

  const DeviceAuthFetchResult& result = future.Get();
  EXPECT_EQ(result.status(), DeviceAuthFetchResult::Status::kError);
}

// Test that unparsable response proto returns Status::kError.
TEST_F(DeviceAuthorizationServiceImplTest,
       TestGetOrFetchKeysProtoParseErrorReturnsProtoParseError) {
  SignInPrimaryAccount();
  test_url_loader_factory_.AddResponse(kDeviceAuthorizationKeyEndpointUrl,
                                       "invalid_not_a_proto");

  TestFuture<DeviceAuthFetchResult> future;
  service_->GetOrFetchKeys(future.GetCallback());

  const DeviceAuthFetchResult& result = future.Get();
  EXPECT_EQ(result.status(), DeviceAuthFetchResult::Status::kError);
}

// Test that a response with an empty key list returns Status::kError, is not
// cached, and that the next request fetches from the server again.
TEST_F(DeviceAuthorizationServiceImplTest,
       TestGetOrFetchKeysEmptyKeysReturnsErrorAndIsNotCached) {
  const GaiaId gaia_id = SignInPrimaryAccount();
  sync_pb::GetDeviceAuthorizationKeyResponse empty_keys_response;
  empty_keys_response.mutable_device_authorization_keys();
  SetResponseForEndpoint(empty_keys_response);

  TestFuture<DeviceAuthFetchResult> future;
  service_->GetOrFetchKeys(future.GetCallback());

  EXPECT_EQ(future.Get().status(), DeviceAuthFetchResult::Status::kError);
  EXPECT_FALSE(HasCachedKeys(gaia_id));

  SetResponseForEndpoint(CreateSuccessResponse());
  TestFuture<DeviceAuthFetchResult> retry_future;
  service_->GetOrFetchKeys(retry_future.GetCallback());

  EXPECT_EQ(retry_future.Get().status(),
            DeviceAuthFetchResult::Status::kSuccess);
  EXPECT_EQ(test_url_loader_factory_.total_requests(), 2u);
  EXPECT_TRUE(HasCachedKeys(gaia_id));
}

// Test that cached keys with an empty key list are not returned, and keys are
// fetched from the server instead.
TEST_F(DeviceAuthorizationServiceImplTest, TestEmptyCachedKeysAreFetchedAgain) {
  const GaiaId gaia_id = SignInPrimaryAccount();
  CachedDeviceAuthorizationKeys empty_cached_keys;
  empty_cached_keys.set_cache_version(1);
  StoreCachedKeys(gaia_id, empty_cached_keys);
  SetResponseForEndpoint(CreateSuccessResponse());

  TestFuture<DeviceAuthFetchResult> future;
  service_->GetOrFetchKeys(future.GetCallback());

  const DeviceAuthFetchResult& result = future.Get();
  EXPECT_EQ(result.status(), DeviceAuthFetchResult::Status::kSuccess);
  ASSERT_TRUE(result.keys());
  EXPECT_EQ(result.keys()->keys(0).key(), kKeyBytes);
  EXPECT_EQ(test_url_loader_factory_.total_requests(), 1u);
}

// Test that cached keys covering all key versions used by stored passkeys are
// returned without a network request.
TEST_F(DeviceAuthorizationServiceImplTest,
       TestCachedKeysCoveringRequiredVersionsAreReturned) {
  const GaiaId gaia_id = SignInPrimaryAccount();
  StoreCachedKeys(gaia_id, CreateCachedKeys(/*cache_version=*/1, kKeyBytes));
  AddPasskeyEncryptedWithKeyVersion(kKeyProtoVersion);

  TestFuture<DeviceAuthFetchResult> future;
  service_->GetOrFetchKeys(future.GetCallback());

  EXPECT_EQ(future.Get().status(), DeviceAuthFetchResult::Status::kSuccess);
  EXPECT_EQ(test_url_loader_factory_.total_requests(), 0u);
}

// Test that a passkey without a device authorization key version does not
// prevent cached keys from being returned.
TEST_F(DeviceAuthorizationServiceImplTest,
       TestPasskeyWithoutKeyVersionDoesNotInvalidateCachedKeys) {
  static constexpr char kPasskeyId[] = "passkey_without_key_version";
  const GaiaId gaia_id = SignInPrimaryAccount();
  StoreCachedKeys(gaia_id, CreateCachedKeys(/*cache_version=*/1, kKeyBytes));
  sync_pb::WebauthnCredentialSpecifics passkey;
  passkey.set_sync_id(kPasskeyId);
  passkey.set_credential_id(kPasskeyId);
  passkey.set_rp_id(kRpId);
  passkey.set_security_domain_encrypted(kEncryptedPasskeyData);
  passkey_model_.AddNewPasskeyForTesting(std::move(passkey));

  TestFuture<DeviceAuthFetchResult> future;
  service_->GetOrFetchKeys(future.GetCallback());

  EXPECT_EQ(future.Get().status(), DeviceAuthFetchResult::Status::kSuccess);
  EXPECT_EQ(test_url_loader_factory_.total_requests(), 0u);
}

// Test that cached keys missing a key version used by a stored passkey are not
// returned, and that the fetched keys are cached instead.
TEST_F(DeviceAuthorizationServiceImplTest,
       TestCachedKeysMissingRequiredVersionAreFetchedAgain) {
  const GaiaId gaia_id = SignInPrimaryAccount();
  StoreCachedKeys(gaia_id, CreateCachedKeys(/*cache_version=*/1, kKeyBytes));
  AddPasskeyEncryptedWithKeyVersion(kNewerKeyProtoVersion);

  sync_pb::GetDeviceAuthorizationKeyResponse response = CreateSuccessResponse();
  DeviceAuthorizationKey* newer_key =
      response.mutable_device_authorization_keys()->add_keys();
  newer_key->set_version(kNewerKeyProtoVersion);
  newer_key->set_key(kKeyBytes);
  SetResponseForEndpoint(response);

  TestFuture<DeviceAuthFetchResult> future;
  service_->GetOrFetchKeys(future.GetCallback());

  const DeviceAuthFetchResult& result = future.Get();
  EXPECT_EQ(result.status(), DeviceAuthFetchResult::Status::kSuccess);
  ASSERT_TRUE(result.keys());
  EXPECT_THAT(result.keys()->keys(), SizeIs(2));
  EXPECT_EQ(test_url_loader_factory_.total_requests(), 1u);

  TestFuture<std::optional<CachedDeviceAuthorizationKeys>> stored_future;
  client_->GetCachedKeys(gaia_id, stored_future.GetCallback());
  ASSERT_TRUE(stored_future.Get().has_value());
  EXPECT_THAT(stored_future.Get()->keys().keys(), SizeIs(2));
}

// Test that fetched keys missing a key version used by a stored passkey are
// still returned and cached, since they are all keys the server has.
TEST_F(DeviceAuthorizationServiceImplTest,
       TestFetchedKeysMissingRequiredVersionAreReturned) {
  const GaiaId gaia_id = SignInPrimaryAccount();
  AddPasskeyEncryptedWithKeyVersion(kNewerKeyProtoVersion);
  SetResponseForEndpoint(CreateSuccessResponse());

  TestFuture<DeviceAuthFetchResult> future;
  service_->GetOrFetchKeys(future.GetCallback());

  EXPECT_EQ(future.Get().status(), DeviceAuthFetchResult::Status::kSuccess);
  EXPECT_TRUE(HasCachedKeys(gaia_id));
}

// Test that shutting down the service during an in-flight fetch invokes pending
// callbacks with Status::kError.
TEST_F(DeviceAuthorizationServiceImplTest,
       TestShutdownReturnsServiceShutdownError) {
  SignInPrimaryAccount();

  TestFuture<DeviceAuthFetchResult> future;
  service_->GetOrFetchKeys(future.GetCallback());
  EXPECT_FALSE(future.IsReady());

  DestroyService();
  ASSERT_TRUE(future.IsReady());
  const DeviceAuthFetchResult& result = future.Get();
  EXPECT_EQ(result.status(), DeviceAuthFetchResult::Status::kError);
}

// The primary account cannot be changed or cleared on ChromeOS.
#if !BUILDFLAG(IS_CHROMEOS)

// Test that changing the primary account while a fetch is in flight fails the
// pending request, cancels the network request and stores nothing.
TEST_F(DeviceAuthorizationServiceImplTest,
       TestPrimaryAccountChangeCancelsPendingFetch) {
  const GaiaId gaia_id = SignInPrimaryAccount();
  TestFuture<DeviceAuthFetchResult> future;
  service_->GetOrFetchKeys(future.GetCallback());
  ASSERT_TRUE(base::test::RunUntil([&] {
    return test_url_loader_factory_.pending_requests()->size() == 1;
  }));

  const GaiaId other_gaia_id = SignInPrimaryAccount(kOtherEmail);

  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get().status(), DeviceAuthFetchResult::Status::kError);
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  EXPECT_FALSE(HasCachedKeys(gaia_id));
  EXPECT_FALSE(HasCachedKeys(other_gaia_id));
}

// Test that a fetch for the new primary account succeeds after a fetch for the
// previous one was cancelled.
TEST_F(DeviceAuthorizationServiceImplTest,
       TestFetchForNewPrimaryAccountAfterAccountChange) {
  const GaiaId gaia_id = SignInPrimaryAccount();
  TestFuture<DeviceAuthFetchResult> future;
  service_->GetOrFetchKeys(future.GetCallback());
  ASSERT_TRUE(base::test::RunUntil([&] {
    return test_url_loader_factory_.pending_requests()->size() == 1;
  }));

  const GaiaId other_gaia_id = SignInPrimaryAccount(kOtherEmail);
  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get().status(), DeviceAuthFetchResult::Status::kError);

  SetResponseForEndpoint(CreateSuccessResponse());
  TestFuture<DeviceAuthFetchResult> other_future;
  service_->GetOrFetchKeys(other_future.GetCallback());

  EXPECT_EQ(other_future.Get().status(),
            DeviceAuthFetchResult::Status::kSuccess);
  EXPECT_FALSE(HasCachedKeys(gaia_id));
  EXPECT_TRUE(HasCachedKeys(other_gaia_id));
}

// Test that signing out while a fetch is in flight fails the pending request.
TEST_F(DeviceAuthorizationServiceImplTest, TestSignOutCancelsPendingFetch) {
  const GaiaId gaia_id = SignInPrimaryAccount();
  TestFuture<DeviceAuthFetchResult> future;
  service_->GetOrFetchKeys(future.GetCallback());
  ASSERT_TRUE(base::test::RunUntil([&] {
    return test_url_loader_factory_.pending_requests()->size() == 1;
  }));

  identity_test_env_.ClearPrimaryAccount();

  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get().status(), DeviceAuthFetchResult::Status::kError);
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  EXPECT_FALSE(HasCachedKeys(gaia_id));
}

// Test that changing the primary account while an access token is being
// fetched for the previous one does not send a request with either account's
// token.
TEST_F(DeviceAuthorizationServiceImplTest,
       TestPrimaryAccountChangeCancelsPendingAccessTokenRequest) {
  identity_test_env_.SetAutomaticIssueOfAccessTokens(false);
  const GaiaId gaia_id = SignInPrimaryAccount();
  SetResponseForEndpoint(CreateSuccessResponse());
  TestFuture<DeviceAuthFetchResult> future;
  service_->GetOrFetchKeys(future.GetCallback());
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return identity_test_env_.IsAccessTokenRequestPending(); }));

  const GaiaId other_gaia_id = SignInPrimaryAccount(kOtherEmail);

  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get().status(), DeviceAuthFetchResult::Status::kError);
  EXPECT_FALSE(identity_test_env_.IsAccessTokenRequestPending());
  EXPECT_EQ(test_url_loader_factory_.NumPending(), 0);
  EXPECT_EQ(test_url_loader_factory_.total_requests(), 0u);
  EXPECT_FALSE(HasCachedKeys(gaia_id));
  EXPECT_FALSE(HasCachedKeys(other_gaia_id));
}

// Test that signing out while an access token is being fetched fails the
// pending request. On sign-out, the access token request fails before the
// service is notified of the primary account change.
TEST_F(DeviceAuthorizationServiceImplTest,
       TestSignOutWhileAccessTokenPendingFailsFetch) {
  identity_test_env_.SetAutomaticIssueOfAccessTokens(false);
  const GaiaId gaia_id = SignInPrimaryAccount();
  TestFuture<DeviceAuthFetchResult> future;
  service_->GetOrFetchKeys(future.GetCallback());
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return identity_test_env_.IsAccessTokenRequestPending(); }));

  identity_test_env_.ClearPrimaryAccount();

  ASSERT_TRUE(future.IsReady());
  EXPECT_EQ(future.Get().status(), DeviceAuthFetchResult::Status::kError);
  EXPECT_EQ(test_url_loader_factory_.total_requests(), 0u);
  EXPECT_FALSE(HasCachedKeys(gaia_id));
}

#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace webauthn
