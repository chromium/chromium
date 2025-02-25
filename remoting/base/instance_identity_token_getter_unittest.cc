// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/instance_identity_token_getter.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {
constexpr char kTestAudience[] = "audience_for_testing";
constexpr char kTokenBodyResponse[] = "instance_identity_token";
// Matches the URL generated for requests from ComputeEngineServiceClient.
constexpr char kHttpMetadataRequestUrl[] =
    "http://metadata.google.internal/computeMetadata/v1/instance/"
    "service-accounts/default/identity?audience=audience_for_testing&"
    "format=full";
}  // namespace

class InstanceIdentityTokenGetterTest : public testing::Test {
 public:
  InstanceIdentityTokenGetterTest();
  ~InstanceIdentityTokenGetterTest() override;

  void SetUp() override;

  void OnTokenRetrieved(std::string_view token);

 protected:
  void RunUntilQuit();
  void SetTokenResponse(std::string_view response_body);
  void SetErrorResponse(net::HttpStatusCode status);
  void ResetQuitClosure();
  void ClearTokenResponse();
  void FastForwardBy(base::TimeDelta duration);

  InstanceIdentityTokenGetter& instance_identity_token_getter() {
    return *instance_identity_token_getter_;
  }

  void set_pending_callback_count(int count) {
    pending_callback_count_ = count;
  }

  const std::optional<std::string>& token() { return token_; }

  size_t url_loader_request_count() {
    return test_url_loader_factory_.total_requests();
  }

 private:
  int pending_callback_count_ = 1;
  std::optional<std::string> token_;

  base::RepeatingClosure quit_closure_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  std::unique_ptr<InstanceIdentityTokenGetter> instance_identity_token_getter_;
};

InstanceIdentityTokenGetterTest::InstanceIdentityTokenGetterTest() = default;
InstanceIdentityTokenGetterTest::~InstanceIdentityTokenGetterTest() = default;

void InstanceIdentityTokenGetterTest::SetUp() {
  shared_url_loader_factory_ = test_url_loader_factory_.GetSafeWeakWrapper();
  instance_identity_token_getter_ =
      std::make_unique<InstanceIdentityTokenGetter>(kTestAudience,
                                                    shared_url_loader_factory_);
  quit_closure_ = task_environment_.QuitClosure();
}

void InstanceIdentityTokenGetterTest::RunUntilQuit() {
  task_environment_.RunUntilQuit();
}

void InstanceIdentityTokenGetterTest::SetTokenResponse(
    std::string_view response_body) {
  ClearTokenResponse();
  test_url_loader_factory_.AddResponse(kHttpMetadataRequestUrl, response_body);
}

void InstanceIdentityTokenGetterTest::ClearTokenResponse() {
  test_url_loader_factory_.ClearResponses();
}

void InstanceIdentityTokenGetterTest::SetErrorResponse(
    net::HttpStatusCode status) {
  ClearTokenResponse();
  test_url_loader_factory_.AddResponse(kHttpMetadataRequestUrl,
                                       /*content=*/std::string(), status);
}

void InstanceIdentityTokenGetterTest::ResetQuitClosure() {
  ASSERT_TRUE(quit_closure_.is_null());
  quit_closure_ = task_environment_.QuitClosure();
}

void InstanceIdentityTokenGetterTest::OnTokenRetrieved(std::string_view token) {
  // If the callback has been run previously, make sure each callback receives
  // the same value.
  if (token_.has_value()) {
    ASSERT_EQ(*token_, token);
  } else {
    token_ = token;
  }

  pending_callback_count_--;
  if (pending_callback_count_ == 0) {
    std::move(quit_closure_).Run();
  }
}

void InstanceIdentityTokenGetterTest::FastForwardBy(base::TimeDelta duration) {
  task_environment_.FastForwardBy(duration);
}

TEST_F(InstanceIdentityTokenGetterTest, SingleRequest) {
  SetTokenResponse(kTokenBodyResponse);

  instance_identity_token_getter().RetrieveToken(
      base::BindOnce(&InstanceIdentityTokenGetterTest::OnTokenRetrieved,
                     base::Unretained(this)));

  RunUntilQuit();

  ASSERT_TRUE(token().has_value());
  ASSERT_EQ(*token(), kTokenBodyResponse);
  ASSERT_EQ(url_loader_request_count(), 1U);
}

TEST_F(InstanceIdentityTokenGetterTest, MultipleRequests) {
  const int kQueuedCallbackCount = 10;

  set_pending_callback_count(kQueuedCallbackCount);
  for (int i = 0; i < kQueuedCallbackCount; i++) {
    instance_identity_token_getter().RetrieveToken(
        base::BindOnce(&InstanceIdentityTokenGetterTest::OnTokenRetrieved,
                       base::Unretained(this)));
  }

  SetTokenResponse(kTokenBodyResponse);

  RunUntilQuit();

  ASSERT_TRUE(token().has_value());
  ASSERT_EQ(*token(), kTokenBodyResponse);
  ASSERT_EQ(url_loader_request_count(), 1U);
}

TEST_F(InstanceIdentityTokenGetterTest, CachedTokenReturned) {
  instance_identity_token_getter().RetrieveToken(
      base::BindOnce(&InstanceIdentityTokenGetterTest::OnTokenRetrieved,
                     base::Unretained(this)));

  SetTokenResponse(kTokenBodyResponse);

  RunUntilQuit();

  ASSERT_TRUE(token().has_value());
  ASSERT_EQ(*token(), kTokenBodyResponse);

  // Call a second time and verify a token is provided w/o calling the service.
  ClearTokenResponse();
  ResetQuitClosure();
  set_pending_callback_count(1);

  instance_identity_token_getter().RetrieveToken(
      base::BindOnce(&InstanceIdentityTokenGetterTest::OnTokenRetrieved,
                     base::Unretained(this)));

  RunUntilQuit();

  ASSERT_TRUE(token().has_value());
  ASSERT_EQ(*token(), kTokenBodyResponse);
  ASSERT_EQ(url_loader_request_count(), 1U);
}

TEST_F(InstanceIdentityTokenGetterTest, CachedTokenIgnored) {
  instance_identity_token_getter().RetrieveToken(
      base::BindOnce(&InstanceIdentityTokenGetterTest::OnTokenRetrieved,
                     base::Unretained(this)));

  SetTokenResponse(kTokenBodyResponse);

  RunUntilQuit();

  ASSERT_TRUE(token().has_value());
  ASSERT_EQ(*token(), kTokenBodyResponse);

  // Call again and verify a token is provided after calling the service.
  FastForwardBy(base::Hours(1));
  ResetQuitClosure();
  set_pending_callback_count(1);

  instance_identity_token_getter().RetrieveToken(
      base::BindOnce(&InstanceIdentityTokenGetterTest::OnTokenRetrieved,
                     base::Unretained(this)));

  RunUntilQuit();

  ASSERT_TRUE(token().has_value());
  ASSERT_EQ(*token(), kTokenBodyResponse);
  ASSERT_EQ(url_loader_request_count(), 2U);
}

TEST_F(InstanceIdentityTokenGetterTest, ServiceFailureReturnsEmptyString) {
  instance_identity_token_getter().RetrieveToken(
      base::BindOnce(&InstanceIdentityTokenGetterTest::OnTokenRetrieved,
                     base::Unretained(this)));

  SetErrorResponse(net::HTTP_BAD_REQUEST);

  RunUntilQuit();

  ASSERT_TRUE(token().has_value());
  ASSERT_TRUE(token()->empty());
}

}  // namespace remoting
