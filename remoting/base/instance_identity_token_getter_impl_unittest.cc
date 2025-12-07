// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/base/instance_identity_token_getter_impl.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/base64.h"
#include "base/environment.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "net/http/http_status_code.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace remoting {

namespace {
constexpr char kTestAudience[] = "audience_for_testing";
// Matches the URL generated for requests from ComputeEngineServiceClient.
constexpr char kHttpMetadataRequestUrl[] =
    "http://metadata.google.internal/computeMetadata/v1/instance/"
    "service-accounts/default/identity?audience=audience_for_testing&"
    "format=full";
// The environment variable used to override the default metadata server host
// when code is run on a Compute Engine instance.
constexpr char kGceMetadataHostVarName[] = "GCE_METADATA_HOST";

// Constants used for testing Compute Engine Metadata server overrides.
constexpr char kTestMetadataServerHost[] = "override.google.internal";
constexpr char kOverriddenMetadataRequestUrl[] =
    "http://override.google.internal/computeMetadata/v1/instance/"
    "service-accounts/default/identity?audience=audience_for_testing&"
    "format=full";

// This JWT is valid but decoding will fail if kStrict is used because the
// length of the header and payload are not divisible by 4 as the padding has
// been stripped which is how tokens received from the metadata server are
// formatted.
constexpr std::string_view kJwtWithoutPadding =
    // Header
    "eyJhbGciOiJSUzI1NiIsImtpZCI6ImtpZC1zaWduYXR1cmUiLCJ0eXAiOiJKV1QifQ."
    // Payload
    "eyJhdWQiOiJhdWRpZW5jZV9mb3JfdGVzdGluZyIsImF6cCI6IjEyMzQ1IiwiZXhwIjo1NzQw"
    "MTcsImdvb2dsZSI6eyJjb21wdXRlX2VuZ2luZSI6eyJpbnN0YW5jZV9pZCI6IjEyMzQ1Njc4"
    "OSIsImluc3RhbmNlX25hbWUiOiJteS1pbnN0YW5jZSIsInByb2plY3RfaWQiOiJteS1wcm9q"
    "ZWN0IiwicHJvamVjdF9udW1iZXIiOjU0MzIxLCJ6b25lIjoidXMtd2lsZC13ZXN0MS16In19"
    "LCJpYXQiOjU3MDQxNywiaXNzIjoiRzAwZ2xlIiwic3ViIjoiMTIzNDUifQ."
    // Signature
    "SIGNATURE!!";

base::Value::Dict CreateJwtHeaderDict() {
  return base::Value::Dict()
      .Set("typ", "JWT")
      .Set("kid", "kid-signature")
      .Set("alg", "RS256");
}

base::Value::Dict CreateJwtPayloadDict(base::Time now = base::Time::Now()) {
  auto compute_engine_dict = base::Value::Dict()
                                 .Set("instance_id", "123456789")
                                 .Set("instance_name", "my-instance")
                                 .Set("project_id", "my-project")
                                 .Set("project_number", 54321)
                                 .Set("zone", "us-wild-west1-z");

  return base::Value::Dict()
      .Set("iss", "G00gle")
      .Set("iat", static_cast<int>(now.InSecondsFSinceUnixEpoch()))
      .Set("exp", static_cast<int>(
                      (now + base::Minutes(60)).InSecondsFSinceUnixEpoch()))
      .Set("aud", kTestAudience)
      .Set("sub", "12345")
      .Set("azp", "12345")
      .Set("google", base::Value::Dict().Set("compute_engine",
                                             std::move(compute_engine_dict)));
}

std::string Base64EncodeDict(base::Value::Dict dict) {
  return base::Base64Encode(*base::WriteJson(std::move(dict)));
}

std::string GetBase64EncodedHeader() {
  return Base64EncodeDict(CreateJwtHeaderDict());
}

std::string GetBase64EncodedPayload(base::Time now = base::Time::Now()) {
  return Base64EncodeDict(CreateJwtPayloadDict(now));
}

std::string GenerateValidJwt(std::string header, std::string payload) {
  return header + "." + payload + "." + "signature";
}

std::string GenerateValidJwt(base::Time now = base::Time::Now()) {
  return GenerateValidJwt(GetBase64EncodedHeader(), GetBase64EncodedPayload());
}

struct TokenParams {
  TokenParams(std::string header, std::string payload, std::string test_name)
      : header(std::move(header)),
        payload(std::move(payload)),
        test_name(std::move(test_name)) {}

  std::string header;
  std::string payload;
  std::string test_name;
};

}  // namespace

class InstanceIdentityTokenGetterImplTest
    : public testing::Test,
      public testing::WithParamInterface<TokenParams> {
 public:
  InstanceIdentityTokenGetterImplTest();
  ~InstanceIdentityTokenGetterImplTest() override;

  void SetUp() override;

  void OnTokenRetrieved(std::string_view token);

 protected:
  void RunUntilQuit();
  void SetTokenResponse(
      std::string_view response_body,
      std::string_view metadata_server_url = kHttpMetadataRequestUrl);
  void SetErrorResponse(net::HttpStatusCode status);
  void ResetQuitClosure();
  void ClearTokenResponse();
  void FastForwardBy(base::TimeDelta duration);
  void SetMetadataServerEnvVar(std::string_view metadata_server_host);

  InstanceIdentityTokenGetterImpl& instance_identity_token_getter() {
    return *instance_identity_token_getter_;
  }

  void set_pending_callback_count(int count) {
    pending_callback_count_ = count;
  }

  const std::optional<std::string>& token() { return token_; }
  void clear_token() { token_.reset(); }

  size_t url_loader_request_count() {
    return test_url_loader_factory_.total_requests();
  }

  std::string_view valid_jwt() { return valid_jwt_; }

 private:
  int pending_callback_count_ = 1;
  std::optional<std::string> token_;
  // Generate once and store so this token can be used for comparisons.
  std::string valid_jwt_;

  base::RepeatingClosure quit_closure_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  std::unique_ptr<base::Environment> environment_ = base::Environment::Create();
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  std::unique_ptr<InstanceIdentityTokenGetterImpl>
      instance_identity_token_getter_;
};

InstanceIdentityTokenGetterImplTest::InstanceIdentityTokenGetterImplTest() =
    default;
InstanceIdentityTokenGetterImplTest::~InstanceIdentityTokenGetterImplTest() =
    default;

void InstanceIdentityTokenGetterImplTest::SetUp() {
  valid_jwt_ = GenerateValidJwt();
  shared_url_loader_factory_ = test_url_loader_factory_.GetSafeWeakWrapper();
  // Unset any pre-existing environment vars before constructing the
  // InstanceIdentityTokenGetterImpl so the externally set values are not used.
  environment_->UnSetVar(kGceMetadataHostVarName);
  instance_identity_token_getter_ =
      std::make_unique<InstanceIdentityTokenGetterImpl>(
          kTestAudience, shared_url_loader_factory_);

  quit_closure_ = task_environment_.QuitClosure();
}

void InstanceIdentityTokenGetterImplTest::RunUntilQuit() {
  task_environment_.RunUntilQuit();
}

void InstanceIdentityTokenGetterImplTest::SetTokenResponse(
    std::string_view response_body,
    std::string_view metadata_server_url) {
  ClearTokenResponse();
  test_url_loader_factory_.AddResponse(metadata_server_url, response_body);
}

void InstanceIdentityTokenGetterImplTest::ClearTokenResponse() {
  test_url_loader_factory_.ClearResponses();
}

void InstanceIdentityTokenGetterImplTest::SetErrorResponse(
    net::HttpStatusCode status) {
  ClearTokenResponse();
  test_url_loader_factory_.AddResponse(kHttpMetadataRequestUrl,
                                       /*content=*/std::string(), status);
}

void InstanceIdentityTokenGetterImplTest::ResetQuitClosure() {
  ASSERT_TRUE(quit_closure_.is_null());
  quit_closure_ = task_environment_.QuitClosure();
}

void InstanceIdentityTokenGetterImplTest::OnTokenRetrieved(
    std::string_view token) {
  // If the callback has been run previously, make sure each callback receives
  // the same value.
  if (token_.has_value()) {
    EXPECT_EQ(*token_, token);
  } else {
    token_ = token;
  }

  pending_callback_count_--;
  if (pending_callback_count_ == 0) {
    std::move(quit_closure_).Run();
  }
}

void InstanceIdentityTokenGetterImplTest::FastForwardBy(
    base::TimeDelta duration) {
  task_environment_.FastForwardBy(duration);
}

void InstanceIdentityTokenGetterImplTest::SetMetadataServerEnvVar(
    std::string_view metadata_server_host) {
  environment_->SetVar(kGceMetadataHostVarName,
                       std::string(metadata_server_host));
  // Recreate the InstanceIdentityTokenGetterImpl so the new value is used.
  instance_identity_token_getter_ =
      std::make_unique<InstanceIdentityTokenGetterImpl>(
          kTestAudience, shared_url_loader_factory_);
}

TEST_F(InstanceIdentityTokenGetterImplTest, SingleRequest) {
  SetTokenResponse(valid_jwt());

  instance_identity_token_getter().RetrieveToken(
      base::BindOnce(&InstanceIdentityTokenGetterImplTest::OnTokenRetrieved,
                     base::Unretained(this)));

  RunUntilQuit();

  ASSERT_TRUE(token().has_value());
  ASSERT_EQ(*token(), valid_jwt());
  ASSERT_EQ(url_loader_request_count(), 1U);
}

TEST_F(InstanceIdentityTokenGetterImplTest,
       SingleRequestWithCustomMetadataServer) {
  SetMetadataServerEnvVar(kTestMetadataServerHost);
  SetTokenResponse(valid_jwt(), kOverriddenMetadataRequestUrl);

  instance_identity_token_getter().RetrieveToken(
      base::BindOnce(&InstanceIdentityTokenGetterImplTest::OnTokenRetrieved,
                     base::Unretained(this)));

  RunUntilQuit();

  ASSERT_TRUE(token().has_value());
  ASSERT_EQ(*token(), valid_jwt());
  ASSERT_EQ(url_loader_request_count(), 1U);
}

TEST_F(InstanceIdentityTokenGetterImplTest, JwtWithoutPadding) {
  // Base64 decode will fail if kStrict is used.
  SetTokenResponse(kJwtWithoutPadding);

  instance_identity_token_getter().RetrieveToken(
      base::BindOnce(&InstanceIdentityTokenGetterImplTest::OnTokenRetrieved,
                     base::Unretained(this)));

  RunUntilQuit();

  ASSERT_TRUE(token().has_value());
  ASSERT_EQ(*token(), kJwtWithoutPadding);
  ASSERT_EQ(url_loader_request_count(), 1U);
}

TEST_F(InstanceIdentityTokenGetterImplTest, MultipleRequests) {
  const int kQueuedCallbackCount = 10;

  set_pending_callback_count(kQueuedCallbackCount);
  for (int i = 0; i < kQueuedCallbackCount; i++) {
    instance_identity_token_getter().RetrieveToken(
        base::BindOnce(&InstanceIdentityTokenGetterImplTest::OnTokenRetrieved,
                       base::Unretained(this)));
  }

  SetTokenResponse(valid_jwt());

  RunUntilQuit();

  ASSERT_TRUE(token().has_value());
  ASSERT_EQ(*token(), valid_jwt());
  ASSERT_EQ(url_loader_request_count(), 1U);
}

TEST_F(InstanceIdentityTokenGetterImplTest, CachedTokenReturned) {
  instance_identity_token_getter().RetrieveToken(
      base::BindOnce(&InstanceIdentityTokenGetterImplTest::OnTokenRetrieved,
                     base::Unretained(this)));

  SetTokenResponse(valid_jwt());

  RunUntilQuit();

  ASSERT_TRUE(token().has_value());
  ASSERT_EQ(*token(), valid_jwt());

  // Call a second time and verify a token is provided w/o calling the service.
  ClearTokenResponse();
  ResetQuitClosure();
  set_pending_callback_count(1);

  instance_identity_token_getter().RetrieveToken(
      base::BindOnce(&InstanceIdentityTokenGetterImplTest::OnTokenRetrieved,
                     base::Unretained(this)));

  RunUntilQuit();

  ASSERT_TRUE(token().has_value());
  ASSERT_EQ(*token(), valid_jwt());
  ASSERT_EQ(url_loader_request_count(), 1U);
}

TEST_F(InstanceIdentityTokenGetterImplTest, CachedTokenIgnored) {
  auto first_jwt_response = valid_jwt();
  instance_identity_token_getter().RetrieveToken(
      base::BindOnce(&InstanceIdentityTokenGetterImplTest::OnTokenRetrieved,
                     base::Unretained(this)));

  SetTokenResponse(first_jwt_response);

  RunUntilQuit();

  ASSERT_TRUE(token().has_value());
  ASSERT_EQ(*token(), first_jwt_response);

  // Call again and verify a token is provided after calling the service.
  FastForwardBy(base::Hours(1));
  ResetQuitClosure();
  set_pending_callback_count(1);
  clear_token();
  // Generate a new JWT with updated timestamp.
  auto second_jwt_response = GenerateValidJwt();
  SetTokenResponse(second_jwt_response);

  instance_identity_token_getter().RetrieveToken(
      base::BindOnce(&InstanceIdentityTokenGetterImplTest::OnTokenRetrieved,
                     base::Unretained(this)));

  RunUntilQuit();

  ASSERT_TRUE(token().has_value());
  ASSERT_EQ(*token(), second_jwt_response);
  ASSERT_EQ(url_loader_request_count(), 2U);
}

TEST_F(InstanceIdentityTokenGetterImplTest, ServiceFailureReturnsEmptyString) {
  instance_identity_token_getter().RetrieveToken(
      base::BindOnce(&InstanceIdentityTokenGetterImplTest::OnTokenRetrieved,
                     base::Unretained(this)));

  SetErrorResponse(net::HTTP_BAD_REQUEST);

  RunUntilQuit();

  ASSERT_TRUE(token().has_value());
  ASSERT_TRUE(token()->empty());
}

TEST_F(InstanceIdentityTokenGetterImplTest, UnsignedJwtReturnsEmptyToken) {
  // Set response to a token which is missing the signature.
  SetTokenResponse(GetBase64EncodedHeader() + "." + GetBase64EncodedPayload());

  instance_identity_token_getter().RetrieveToken(
      base::BindOnce(&InstanceIdentityTokenGetterImplTest::OnTokenRetrieved,
                     base::Unretained(this)));

  RunUntilQuit();

  ASSERT_TRUE(token().has_value());
  ASSERT_TRUE(token()->empty());
}

TEST_P(InstanceIdentityTokenGetterImplTest, InvalidJwtReturnsEmptyToken) {
  SetTokenResponse(GetParam().header + "." + GetParam().payload + ".signature");

  instance_identity_token_getter().RetrieveToken(
      base::BindOnce(&InstanceIdentityTokenGetterImplTest::OnTokenRetrieved,
                     base::Unretained(this)));

  RunUntilQuit();

  ASSERT_TRUE(token().has_value());
  ASSERT_TRUE(token()->empty());
}

INSTANTIATE_TEST_SUITE_P(
    InstanceIdentityTokenGetterImplTest,
    InstanceIdentityTokenGetterImplTest,
    ::testing::Values(
        TokenParams("", "", "EmptyHeaderAndPayload"),
        TokenParams("a." + GetBase64EncodedHeader(),
                    GetBase64EncodedPayload(),
                    "ExtraTokenSegment"),
        TokenParams(GetBase64EncodedHeader() + "====",
                    GetBase64EncodedPayload() + "====",
                    "ExtraPadding"),
        TokenParams("", GetBase64EncodedPayload(), "EmptyHeader"),
        TokenParams(GetBase64EncodedHeader(), "", "EmptyPayload"),
        TokenParams(Base64EncodeDict(base::Value::Dict()),
                    GetBase64EncodedPayload(),
                    "HeaderIsAnEmptyDict"),
        TokenParams(GetBase64EncodedHeader(),
                    Base64EncodeDict(base::Value::Dict()),
                    "PayloadIsAnEmptyDict"),
        TokenParams(*base::WriteJson(CreateJwtHeaderDict()),
                    GetBase64EncodedPayload(),
                    "HeaderNotBase64Encoded"),
        TokenParams(GetBase64EncodedHeader(),
                    *base::WriteJson(CreateJwtPayloadDict()),
                    "PayloadNotBase64Encoded"),
        TokenParams(base::Base64Encode("I'm JSON!"),
                    GetBase64EncodedPayload(),
                    "HeaderIsNotValidJson"),
        TokenParams(GetBase64EncodedHeader(),
                    base::Base64Encode("I'm JSON!"),
                    "PayloadIsNotValidJson"),
        TokenParams(Base64EncodeDict(base::Value::Dict()
                                         .Set("alg", "RS256")
                                         .Set("typ", "JWT")),
                    GetBase64EncodedPayload(),
                    "HeaderIsMissingMembers"),
        TokenParams(GetBase64EncodedHeader(),
                    Base64EncodeDict(base::Value::Dict().Set("iss", "blergh")),
                    "PayloadIsMissingMembers"),
        TokenParams(GetBase64EncodedHeader(),
                    Base64EncodeDict(CreateJwtPayloadDict().SetByDottedPath(
                        "google",
                        base::Value::Dict())),
                    "NoComputeEngineDict"),
        TokenParams(GetBase64EncodedHeader(),
                    Base64EncodeDict(CreateJwtPayloadDict().SetByDottedPath(
                        "google.compute_engine",
                        base::Value::Dict())),
                    "EmptyComputeEngineDict"),
        TokenParams(GetBase64EncodedHeader(),
                    Base64EncodeDict(CreateJwtPayloadDict().SetByDottedPath(
                        "google.compute_engine",
                        base::Value::Dict().Set("instance_id",
                                                "test-instance-id"))),
                    "ComputeEngineDictMissingValues")),
    [](const testing::TestParamInfo<
        InstanceIdentityTokenGetterImplTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace remoting
