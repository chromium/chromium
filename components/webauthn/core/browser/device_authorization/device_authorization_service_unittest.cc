// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/version_info/channel.h"
#include "components/webauthn/core/browser/device_authorization/device_authorization_client.h"
#include "components/webauthn/core/browser/device_authorization/device_authorization_service_impl.h"
#include "components/webauthn/core/browser/device_authorization/device_authorization_switches.h"
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
constexpr char kKeyBytes[] = "fake_device_auth_key";
constexpr int32_t kKeyProtoVersion = 1;
constexpr char kFakeWebFallbackUrl[] = "https://example.com/reauth";
constexpr char kCustomRapt[] = "custom_rapt_token";

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

class TestDeviceAuthorizationClient : public DeviceAuthorizationClient {
 public:
  TestDeviceAuthorizationClient() = default;
  ~TestDeviceAuthorizationClient() override = default;

  std::optional<DeviceAuthorizationKeys> GetCachedKeys(
      const GaiaId& gaia_id) override {
    auto it = storage_.find(gaia_id);
    if (it == storage_.end()) {
      return std::nullopt;
    }
    return it->second;
  }

  bool StoreKeys(const GaiaId& gaia_id,
                 const DeviceAuthorizationKeys& keys) override {
    storage_[gaia_id] = keys;
    return true;
  }

  void PopulatePlatformData(sync_pb::GetDeviceAuthorizationKeyRequest request,
                            PopulatePlatformDataCallback callback) override {
    request.MergeFrom(request_);
    std::move(callback).Run(std::move(request));
  }

  void set_request(sync_pb::GetDeviceAuthorizationKeyRequest request) {
    request_ = std::move(request);
  }

 private:
  std::map<GaiaId, DeviceAuthorizationKeys> storage_;
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
    auto client = std::make_unique<TestDeviceAuthorizationClient>();
    client_ = client.get();
    service_ = std::make_unique<DeviceAuthorizationServiceImpl>(
        identity_test_env_.identity_manager(), shared_url_loader_factory_,
        std::move(client), version_info::Channel::UNKNOWN);
  }

  void TearDown() override {
    client_ = nullptr;
    service_->Shutdown();
    service_.reset();
  }

  GaiaId SignInPrimaryAccount() {
    identity_test_env_.MakePrimaryAccountAvailable(
        kTestEmail, signin::ConsentLevel::kSignin);
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
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

  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;
  raw_ptr<TestDeviceAuthorizationClient> client_ = nullptr;
  std::unique_ptr<DeviceAuthorizationServiceImpl> service_;
};

// Test that when keys are already cached, GetOrFetchKeys returns them
// immediately without network fetch.
TEST_F(DeviceAuthorizationServiceImplTest,
       TestGetCachedKeysImmediatelyReturned) {
  GaiaId gaia_id = SignInPrimaryAccount();

  sync_pb::GetDeviceAuthorizationKeyResponse::DeviceAuthorizationKeys
      cached_keys;
  auto* key = cached_keys.add_keys();
  key->set_version(kKeyProtoVersion);
  key->set_key(kKeyBytes);
  client_->StoreKeys(gaia_id, cached_keys);

  TestFuture<DeviceAuthFetchResult> future;
  service_->GetOrFetchKeys(future.GetCallback());

  ASSERT_TRUE(future.IsReady());
  const DeviceAuthFetchResult& result = future.Get();
  EXPECT_EQ(result.status(), DeviceAuthFetchResult::Status::kSuccess);
  ASSERT_TRUE(result.keys());
  EXPECT_THAT(result.keys()->keys(), SizeIs(1));
  EXPECT_EQ(result.keys()->keys(0).key(), kKeyBytes);
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

  std::optional<DeviceAuthorizationKeys> stored =
      client_->GetCachedKeys(gaia_id);
  ASSERT_TRUE(stored.has_value());
  EXPECT_THAT(stored->keys(), SizeIs(1));
  EXPECT_EQ(stored->keys(0).key(), kKeyBytes);
}

// Test that a concurrent call to `GetOrFetchKeys` while a fetch is already in
// progress fails immediately with `kError`, while the in-flight fetch
// completes successfully.
TEST_F(DeviceAuthorizationServiceImplTest,
       TestGetOrFetchKeysConcurrentCallReturnsError) {
  SignInPrimaryAccount();
  SetResponseForEndpoint(CreateSuccessResponse());

  TestFuture<DeviceAuthFetchResult> future1;
  TestFuture<DeviceAuthFetchResult> future2;

  service_->GetOrFetchKeys(future1.GetCallback());

  // Second call fails immediately because a fetch is already in progress.
  service_->GetOrFetchKeys(future2.GetCallback());
  ASSERT_TRUE(future2.IsReady());
  EXPECT_EQ(future2.Get().status(), DeviceAuthFetchResult::Status::kError);

  // First call finishes successfully when the response arrives.
  const DeviceAuthFetchResult& result1 = future1.Get();
  EXPECT_EQ(result1.status(), DeviceAuthFetchResult::Status::kSuccess);
  ASSERT_TRUE(result1.keys());
  EXPECT_THAT(result1.keys()->keys(), SizeIs(1));
  EXPECT_EQ(result1.keys()->keys(0).key(), kKeyBytes);
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
  EXPECT_FALSE(client_->GetCachedKeys(gaia_id).has_value());
}

// Test that passing `reauth_proof_token` bypasses cached keys, passes the token
// in the request to the network fetcher, and stores the newly fetched keys.
TEST_F(DeviceAuthorizationServiceImplTest,
       TestFetchKeysWithReAuthTokenBypassesCache) {
  GaiaId gaia_id = SignInPrimaryAccount();

  // Cache existing keys first.
  sync_pb::GetDeviceAuthorizationKeyResponse::DeviceAuthorizationKeys
      cached_keys;
  auto* key = cached_keys.add_keys();
  key->set_version(kKeyProtoVersion);
  key->set_key("old_cached_key");
  client_->StoreKeys(gaia_id, cached_keys);

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
  std::optional<DeviceAuthorizationKeys> stored =
      client_->GetCachedKeys(gaia_id);
  ASSERT_TRUE(stored.has_value());
  EXPECT_EQ(stored->keys(0).key(), kKeyBytes);
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

}  // namespace webauthn
