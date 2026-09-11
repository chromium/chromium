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

  void SetDeviceAuthorizationRequest(
      sync_pb::GetDeviceAuthorizationKeyRequest request) {
    request_ = std::move(request);
  }

  void CreateDeviceAuthorizationRequest(
      CreateDeviceAuthRequestCallback callback) override {
    std::move(callback).Run(request_);
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

  TestFuture<std::optional<DeviceAuthorizationKeys>> future;
  service_->GetOrFetchKeys(future.GetCallback());

  ASSERT_TRUE(future.IsReady());
  const std::optional<DeviceAuthorizationKeys>& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(result->keys(), SizeIs(1));
  EXPECT_EQ(result->keys(0).key(), kKeyBytes);
}

// Test that getting or fetching keys fails when no primary account is signed
// in.
TEST_F(DeviceAuthorizationServiceImplTest, TestNotSignedInFails) {
  TestFuture<std::optional<DeviceAuthorizationKeys>> future;
  service_->GetOrFetchKeys(future.GetCallback());

  ASSERT_TRUE(future.IsReady());
  EXPECT_FALSE(future.Get().has_value());
}

// Test that a cache miss initiates a network request, successfully receives
// keys, persists them to storage, and returns them to the caller.
TEST_F(DeviceAuthorizationServiceImplTest,
       TestGetOrFetchKeysSuccessAndPersist) {
  GaiaId gaia_id = SignInPrimaryAccount();
  SetResponseForEndpoint(CreateSuccessResponse());

  TestFuture<std::optional<DeviceAuthorizationKeys>> future;
  service_->GetOrFetchKeys(future.GetCallback());

  const std::optional<DeviceAuthorizationKeys>& result = future.Get();
  ASSERT_TRUE(result.has_value());
  EXPECT_THAT(result->keys(), SizeIs(1));
  EXPECT_EQ(result->keys(0).key(), kKeyBytes);

  std::optional<DeviceAuthorizationKeys> stored =
      client_->GetCachedKeys(gaia_id);
  ASSERT_TRUE(stored.has_value());
  EXPECT_THAT(stored->keys(), SizeIs(1));
  EXPECT_EQ(stored->keys(0).key(), kKeyBytes);
}

// Test that a concurrent call to GetOrFetchKeys while a fetch is already in
// progress fails immediately with std::nullopt, while the in-flight fetch
// completes successfully.
TEST_F(DeviceAuthorizationServiceImplTest,
       TestGetOrFetchKeysConcurrentCallReturnsNullopt) {
  SignInPrimaryAccount();
  SetResponseForEndpoint(CreateSuccessResponse());

  TestFuture<std::optional<DeviceAuthorizationKeys>> future1;
  TestFuture<std::optional<DeviceAuthorizationKeys>> future2;

  service_->GetOrFetchKeys(future1.GetCallback());

  // Second call fails immediately because a fetch is already in progress.
  service_->GetOrFetchKeys(future2.GetCallback());
  ASSERT_TRUE(future2.IsReady());
  EXPECT_FALSE(future2.Get().has_value());

  // First call finishes successfully when the response arrives.
  const std::optional<DeviceAuthorizationKeys>& result1 = future1.Get();
  ASSERT_TRUE(result1.has_value());
  EXPECT_THAT(result1->keys(), SizeIs(1));
  EXPECT_EQ(result1->keys(0).key(), kKeyBytes);
}

// Test that when the server returns a response without device authorization
// keys (e.g. re_auth_params), the service reports failure and does not cache.
TEST_F(DeviceAuthorizationServiceImplTest,
       TestGetOrFetchKeysServerReturnsNonKeysResponseReturnsNullopt) {
  GaiaId gaia_id = SignInPrimaryAccount();
  SetResponseForEndpoint(CreateReAuthResponse());

  TestFuture<std::optional<DeviceAuthorizationKeys>> future;
  service_->GetOrFetchKeys(future.GetCallback());

  const std::optional<DeviceAuthorizationKeys>& result = future.Get();
  EXPECT_FALSE(result.has_value());
  EXPECT_FALSE(client_->GetCachedKeys(gaia_id).has_value());
}

// Test that GetOrFetchKeys invokes CreateDeviceAuthorizationRequest and
// forwards the client-created request to the network fetcher.
TEST_F(DeviceAuthorizationServiceImplTest,
       TestGetOrFetchKeysSendsCreatedRequestWithCustomFields) {
  SignInPrimaryAccount();
  SetResponseForEndpoint(CreateSuccessResponse());

  std::string intercepted_body;
  test_url_loader_factory_.SetInterceptor(
      base::BindLambdaForTesting([&](const network::ResourceRequest& req) {
        intercepted_body = network::GetUploadData(req);
      }));

  sync_pb::GetDeviceAuthorizationKeyRequest request;
  request.set_reauth_proof_token(kCustomRapt);
  client_->SetDeviceAuthorizationRequest(std::move(request));

  TestFuture<std::optional<DeviceAuthorizationKeys>> future;
  service_->GetOrFetchKeys(future.GetCallback());

  const std::optional<DeviceAuthorizationKeys>& result = future.Get();
  ASSERT_TRUE(result.has_value());

  ASSERT_EQ(test_url_loader_factory_.total_requests(), 1u);
  sync_pb::GetDeviceAuthorizationKeyRequest sent_request;
  ASSERT_TRUE(sent_request.ParseFromString(intercepted_body));
  EXPECT_EQ(sent_request.reauth_proof_token(), kCustomRapt);
}

}  // namespace webauthn
