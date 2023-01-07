// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A set of unit tests for TokenValidatorFactoryImpl

#include "remoting/host/token_validator_factory_impl.h"

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/http/http_status_code.h"
#include "net/test/url_request/url_request_failed_job.h"
#include "net/url_request/url_request_filter.h"
#include "net/url_request/url_request_interceptor.h"
#include "net/url_request/url_request_test_job.h"
#include "net/url_request/url_request_test_util.h"
#include "remoting/base/rsa_key_pair.h"
#include "remoting/base/test_rsa_key_pair.h"
#include "remoting/protocol/token_validator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

const char kTokenUrl[] = "https://example.com/token";
const char kTokenValidationUrl[] = "https://example.com/validate";
const char kTokenValidationCertIssuer[] = "";
const char kLocalJid[] = "user@example.com/local";
const char kRemoteJid[] = "user@example.com/remote";
const char kToken[] = "xyz123456";
const char kSharedSecret[] = "abcdefgh";

// Bad scope: no nonce element.
const char kBadScope[] =
    "client:user@example.com/local host:user@example.com/remote";

class TestURLRequestInterceptor : public net::URLRequestInterceptor {
 public:
  TestURLRequestInterceptor(const std::string& headers,
                            const std::string& response)
      : headers_(headers), response_(response) {}

  ~TestURLRequestInterceptor() override = default;

  std::unique_ptr<net::URLRequestJob> MaybeInterceptRequest(
      net::URLRequest* request) const override {
    return std::make_unique<net::URLRequestTestJob>(request, headers_,
                                                    response_, true);
  }

 private:
  std::string headers_;
  std::string response_;
};

// Creates URLRequestJobs that fail at the specified phase.
class TestFailingURLRequestInterceptor : public net::URLRequestInterceptor {
 public:
  TestFailingURLRequestInterceptor(
      net::URLRequestFailedJob::FailurePhase failure_phase,
      net::Error net_error)
      : failure_phase_(failure_phase), net_error_(net_error) {}

  ~TestFailingURLRequestInterceptor() override = default;

  std::unique_ptr<net::URLRequestJob> MaybeInterceptRequest(
      net::URLRequest* request) const override {
    return std::make_unique<net::URLRequestFailedJob>(request, failure_phase_,
                                                      net_error_);
  }

 private:
  const net::URLRequestFailedJob::FailurePhase failure_phase_;
  const net::Error net_error_;
};

}  // namespace

namespace remoting {

class TokenValidatorFactoryImplTest : public testing::Test {
 public:
  TokenValidatorFactoryImplTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::IO) {}

  ~TokenValidatorFactoryImplTest() override {
    net::URLRequestFilter::GetInstance()->ClearHandlers();
  }

  void SuccessCallback(
      const protocol::TokenValidator::ValidationResult& validation_result) {
    EXPECT_TRUE(validation_result.is_success());
    EXPECT_FALSE(validation_result.is_error());
    EXPECT_TRUE(!validation_result.success().empty());
    run_loop_.QuitWhenIdle();
  }

  void FailureCallback(
      const protocol::TokenValidator::ValidationResult& validation_result) {
    EXPECT_TRUE(validation_result.is_error());
    EXPECT_FALSE(validation_result.is_success());
    run_loop_.QuitWhenIdle();
  }

  void DeleteOnFailureCallback(
      const protocol::TokenValidator::ValidationResult& validation_result) {
    EXPECT_TRUE(validation_result.is_error());
    EXPECT_FALSE(validation_result.is_success());
    token_validator_.reset();
    run_loop_.QuitWhenIdle();
  }

 protected:
  void SetUp() override {
    key_pair_ = RsaKeyPair::FromString(kTestRsaKeyPair);
    request_context_getter_ = new net::TestURLRequestContextGetter(
        task_environment_.GetMainThreadTaskRunner());
    ThirdPartyAuthConfig config;
    config.token_url = GURL(kTokenUrl);
    config.token_validation_url = GURL(kTokenValidationUrl);
    config.token_validation_cert_issuer = kTokenValidationCertIssuer;
    token_validator_factory_ = new TokenValidatorFactoryImpl(
        config, key_pair_, request_context_getter_);
  }

  static std::string CreateResponse(const std::string& scope) {
    base::Value::Dict response_dict;
    response_dict.Set("access_token", kSharedSecret);
    response_dict.Set("token_type", "shared_secret");
    response_dict.Set("scope", scope);
    std::string response;
    base::JSONWriter::Write(response_dict, &response);
    return response;
  }

  static std::string CreateErrorResponse(const std::string& error) {
    base::Value::Dict response_dict;
    response_dict.Set("error", error);
    std::string response;
    base::JSONWriter::Write(response_dict, &response);
    return response;
  }

  void SetResponse(const std::string& headers, const std::string& response) {
    net::URLRequestFilter::GetInstance()->AddHostnameInterceptor(
        "https", "example.com",
        std::make_unique<TestURLRequestInterceptor>(headers, response));
  }

  void SetErrorResponse(net::URLRequestFailedJob::FailurePhase failure_phase,
                        net::Error net_error) {
    net::URLRequestFilter::GetInstance()->AddHostnameInterceptor(
        "https", "example.com",
        std::make_unique<TestFailingURLRequestInterceptor>(failure_phase,
                                                           net_error));
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  base::RunLoop run_loop_;
  scoped_refptr<RsaKeyPair> key_pair_;
  scoped_refptr<net::URLRequestContextGetter> request_context_getter_;
  scoped_refptr<TokenValidatorFactoryImpl> token_validator_factory_;
  std::unique_ptr<protocol::TokenValidator> token_validator_;
};

TEST_F(TokenValidatorFactoryImplTest, Success) {
  token_validator_ =
      token_validator_factory_->CreateTokenValidator(kLocalJid, kRemoteJid);

  SetResponse(net::URLRequestTestJob::test_headers(),
              CreateResponse(token_validator_->token_scope()));

  token_validator_->ValidateThirdPartyToken(
      kToken, base::BindOnce(&TokenValidatorFactoryImplTest::SuccessCallback,
                             base::Unretained(this)));
  run_loop_.Run();
}

TEST_F(TokenValidatorFactoryImplTest,
       ValidResponseWithJsonSafetyPrefix_Success) {
  token_validator_ =
      token_validator_factory_->CreateTokenValidator(kLocalJid, kRemoteJid);

  SetResponse(net::URLRequestTestJob::test_headers(),
              ")]}'\n" + CreateResponse(token_validator_->token_scope()));

  token_validator_->ValidateThirdPartyToken(
      kToken, base::BindOnce(&TokenValidatorFactoryImplTest::SuccessCallback,
                             base::Unretained(this)));
  run_loop_.Run();
}

TEST_F(TokenValidatorFactoryImplTest, BadToken) {
  token_validator_ =
      token_validator_factory_->CreateTokenValidator(kLocalJid, kRemoteJid);

  SetResponse(net::URLRequestTestJob::test_error_headers(), std::string());

  token_validator_->ValidateThirdPartyToken(
      kToken, base::BindOnce(&TokenValidatorFactoryImplTest::FailureCallback,
                             base::Unretained(this)));
  run_loop_.Run();
}

TEST_F(TokenValidatorFactoryImplTest, BadScope) {
  token_validator_ =
      token_validator_factory_->CreateTokenValidator(kLocalJid, kRemoteJid);

  SetResponse(net::URLRequestTestJob::test_headers(),
              CreateResponse(kBadScope));

  token_validator_->ValidateThirdPartyToken(
      kToken, base::BindOnce(&TokenValidatorFactoryImplTest::FailureCallback,
                             base::Unretained(this)));
  run_loop_.Run();
}

TEST_F(TokenValidatorFactoryImplTest, DeleteOnFailure) {
  token_validator_ =
      token_validator_factory_->CreateTokenValidator(kLocalJid, kRemoteJid);

  SetResponse(net::URLRequestTestJob::test_error_headers(), std::string());

  token_validator_->ValidateThirdPartyToken(
      kToken,
      base::BindOnce(&TokenValidatorFactoryImplTest::DeleteOnFailureCallback,
                     base::Unretained(this)));
  run_loop_.Run();
}

TEST_F(TokenValidatorFactoryImplTest, DeleteOnStartError) {
  token_validator_ =
      token_validator_factory_->CreateTokenValidator(kLocalJid, kRemoteJid);

  SetErrorResponse(net::URLRequestFailedJob::START, net::ERR_FAILED);

  token_validator_->ValidateThirdPartyToken(
      kToken,
      base::BindOnce(&TokenValidatorFactoryImplTest::DeleteOnFailureCallback,
                     base::Unretained(this)));
  run_loop_.Run();
}

TEST_F(TokenValidatorFactoryImplTest, DeleteOnSyncReadError) {
  token_validator_ =
      token_validator_factory_->CreateTokenValidator(kLocalJid, kRemoteJid);

  SetErrorResponse(net::URLRequestFailedJob::READ_SYNC, net::ERR_FAILED);

  token_validator_->ValidateThirdPartyToken(
      kToken,
      base::BindOnce(&TokenValidatorFactoryImplTest::DeleteOnFailureCallback,
                     base::Unretained(this)));
  run_loop_.Run();
}

TEST_F(TokenValidatorFactoryImplTest, DeleteOnAsyncReadError) {
  token_validator_ =
      token_validator_factory_->CreateTokenValidator(kLocalJid, kRemoteJid);

  SetErrorResponse(net::URLRequestFailedJob::READ_ASYNC, net::ERR_FAILED);

  token_validator_->ValidateThirdPartyToken(
      kToken,
      base::BindOnce(&TokenValidatorFactoryImplTest::DeleteOnFailureCallback,
                     base::Unretained(this)));
  run_loop_.Run();
}

}  // namespace remoting
