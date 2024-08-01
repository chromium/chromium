// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_key_commitment_controller.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "net/url_request/url_request_test_util.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/mojom/trust_tokens.mojom-forward.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "services/network/trust_tokens/trust_token_key_commitment_controller.h"
#include "services/network/trust_tokens/trust_token_parameterization.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using ::testing::Optional;

namespace network {

using internal::CreateTrustTokenKeyCommitmentRequest;

namespace {

class FixedKeyCommitmentParser
    : public TrustTokenKeyCommitmentController::Parser {
 public:
  mojom::TrustTokenKeyCommitmentResultPtr Parse(
      std::string_view response_body) override {
    return DeterministicallyReturnedValue();
  }
  static mojom::TrustTokenKeyCommitmentResultPtr
  DeterministicallyReturnedValue() {
    auto result = mojom::TrustTokenKeyCommitmentResult::New();
    result->batch_size = 10;
    return result;
  }
};

class FailingKeyCommitmentParser
    : public TrustTokenKeyCommitmentController::Parser {
  mojom::TrustTokenKeyCommitmentResultPtr Parse(
      std::string_view response_body) override {
    return nullptr;
  }
};

GURL IssuerDotComKeyCommitmentPath() {
  return GURL("https://issuer.com")
      .Resolve(kTrustTokenKeyCommitmentWellKnownPath);
}

class CommitmentWaiter {
 public:
  base::OnceCallback<void(TrustTokenKeyCommitmentController::Status,
                          mojom::TrustTokenKeyCommitmentResultPtr)>
  Callback() {
    return base::BindOnce(&CommitmentWaiter::OnComplete,
                          base::Unretained(this));
  }

  std::pair<TrustTokenKeyCommitmentController::Status,
            mojom::TrustTokenKeyCommitmentResultPtr>
  WaitForResult() {
    run_loop_.Run();
    CHECK(done_);
    return std::make_pair(status_, std::move(result_));
  }

 private:
  void OnComplete(TrustTokenKeyCommitmentController::Status status,
                  mojom::TrustTokenKeyCommitmentResultPtr result) {
    done_ = true;
    status_ = status;
    result_ = std::move(result);
    run_loop_.Quit();
  }

  bool done_ = false;
  TrustTokenKeyCommitmentController::Status status_;
  mojom::TrustTokenKeyCommitmentResultPtr result_;
  base::RunLoop run_loop_;
};

}  // namespace

// Use a fixture just to reduce the amount of boilerplate it takes to create an
// empty URLRequest.
class TrustTokenKeyCommitmentControllerTest : public ::testing::Test {
 public:
  TrustTokenKeyCommitmentControllerTest()
      : context_(net::CreateTestURLRequestContextBuilder()->Build()),
        issuer_request_(MakeURLRequest("https://issuer.com/")) {}
  ~TrustTokenKeyCommitmentControllerTest() override = default;

 protected:
  std::unique_ptr<net::URLRequest> MakeURLRequest(std::string spec) {
    return context_->CreateRequest(GURL(spec),
                                   net::RequestPriority::DEFAULT_PRIORITY,
                                   &delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);
  }
  const net::URLRequest& IssuerURLRequest() { return *issuer_request_; }
  base::test::TaskEnvironment env_;
  net::TestDelegate delegate_;
  std::unique_ptr<net::URLRequestContext> context_;
  std::unique_ptr<net::URLRequest> issuer_request_;
};

// Test that CreateTrustTokenKeyCommitmentRequest satisfies the contract
// provided by its function comment:
TEST_F(TrustTokenKeyCommitmentControllerTest, CreatedRequestsBypassCache) {
  // (Here and below, this is the pertinent snippet from the function
  // comment in key_commitment_controller.h.)
  //
  // 1. sets the LOAD_BYPASS_CACHE and LOAD_DISABLE_CACHE flags,
  // so that the result doesn't check the cache and isn't cached itself
  std::unique_ptr<ResourceRequest> request =
      CreateTrustTokenKeyCommitmentRequest(
          IssuerURLRequest(),
          url::Origin::Create(GURL("https://toplevel.com")));

  EXPECT_TRUE(request->load_flags & net::LOAD_BYPASS_CACHE);
  EXPECT_TRUE(request->load_flags & net::LOAD_DISABLE_CACHE);
}

TEST_F(TrustTokenKeyCommitmentControllerTest,
       CreatedRequestsAreUncredentialed) {
  // 3. sets the key commitment request to be uncredentialed
  std::unique_ptr<ResourceRequest> request =
      CreateTrustTokenKeyCommitmentRequest(
          IssuerURLRequest(),
          url::Origin::Create(GURL("https://toplevel.com")));

  EXPECT_EQ(request->credentials_mode, mojom::CredentialsMode::kOmit);
}

TEST_F(TrustTokenKeyCommitmentControllerTest,
       CreatedRequestsCopyUnderlyingRequestsInitiators) {
  // 4. copies |request|'s initiator to the key commitment request
  auto url_request = MakeURLRequest("");
  url_request->set_initiator(
      url::Origin::Create(GURL("https://initiator.com")));

  std::unique_ptr<ResourceRequest> request =
      CreateTrustTokenKeyCommitmentRequest(
          *url_request, url::Origin::Create(GURL("https://toplevel.com")));

  EXPECT_THAT(request->request_initiator,
              Optional(url::Origin::Create(GURL("https://initiator.com"))));
}

TEST_F(TrustTokenKeyCommitmentControllerTest,
       CreatedRequestsSetOriginToProvidedTopLevel) {
  // 5. sets the key commitment request's Origin header to equal
  // |request|'s top-level origin.
  std::unique_ptr<ResourceRequest> request =
      CreateTrustTokenKeyCommitmentRequest(
          IssuerURLRequest(),
          url::Origin::Create(GURL("https://toplevel.com")));

  EXPECT_THAT(request->headers.GetHeader(net::HttpRequestHeaders::kOrigin),
              Optional(std::string("https://toplevel.com")));
}

// On network error, the key commitment controller should
// pass the underlying error code upstream.
TEST_F(TrustTokenKeyCommitmentControllerTest, NetworkError) {
  TestURLLoaderFactory factory;
  factory.AddResponse(
      IssuerDotComKeyCommitmentPath(), mojom::URLResponseHead::New(),
      /*content=*/"",
      URLLoaderCompletionStatus(
          net::ERR_CONNECTION_REFUSED /* chosen arbitrarily */));

  CommitmentWaiter waiter;

  auto url_request = MakeURLRequest("https://issuer.com/");

  TrustTokenKeyCommitmentController my_controller(
      waiter.Callback(), *url_request,
      url::Origin::Create(GURL("https://toplevel.com/")),
      TRAFFIC_ANNOTATION_FOR_TESTS, &factory,
      std::make_unique<FixedKeyCommitmentParser>());

  auto [result_status, result] = waiter.WaitForResult();
  EXPECT_EQ(result_status.value,
            TrustTokenKeyCommitmentController::Status::Value::kNetworkError);
  EXPECT_EQ(result_status.net_error, net::ERR_CONNECTION_REFUSED);
}

// On a failed parse (emulated by FailingKeyCommitmentParser) after a
// successful network request, the controller should return
// kCouldntParse.
TEST_F(TrustTokenKeyCommitmentControllerTest, NetworkSuccessParseFailure) {
  TestURLLoaderFactory factory;
  factory.AddResponse(IssuerDotComKeyCommitmentPath().spec(), "");

  CommitmentWaiter waiter;

  auto url_request = MakeURLRequest("https://issuer.com/");

  TrustTokenKeyCommitmentController my_controller(
      waiter.Callback(), *url_request,
      url::Origin::Create(GURL("https://toplevel.com/")),
      TRAFFIC_ANNOTATION_FOR_TESTS, &factory,
      std::make_unique<FailingKeyCommitmentParser>());

  auto [result_status, result] = waiter.WaitForResult();
  EXPECT_EQ(result_status.value,
            TrustTokenKeyCommitmentController::Status::Value::kCouldntParse);
}

// On a redirect, the controller should fail with kGotRedirected.
TEST_F(TrustTokenKeyCommitmentControllerTest, Redirect) {
  TestURLLoaderFactory factory;
  factory.AddResponse(IssuerDotComKeyCommitmentPath().spec(), "", net::HTTP_OK);

  net::RedirectInfo redirect_info;
  redirect_info.status_code = 301;
  redirect_info.new_url = GURL("https://unused-redirect-destination.com/");
  network::TestURLLoaderFactory::Redirects redirects;
  redirects.emplace_back(redirect_info, network::mojom::URLResponseHead::New());
  auto head = network::CreateURLResponseHead(net::HTTP_OK);
  factory.AddResponse(IssuerDotComKeyCommitmentPath(), std::move(head),
                      /*content=*/"", network::URLLoaderCompletionStatus(),
                      std::move(redirects));

  CommitmentWaiter waiter;

  auto url_request = MakeURLRequest("https://issuer.com/");

  TrustTokenKeyCommitmentController my_controller(
      waiter.Callback(), *url_request,
      url::Origin::Create(GURL("https://toplevel.com/")),
      TRAFFIC_ANNOTATION_FOR_TESTS, &factory,
      std::make_unique<FailingKeyCommitmentParser>());

  auto [result_status, result] = waiter.WaitForResult();
  EXPECT_EQ(result_status.value,
            TrustTokenKeyCommitmentController::Status::Value::kGotRedirected);
}

// On successful response and successful parse of the result,
// we should see net::OK and a non-null TrustTokenKeyCommitmentResult.
TEST_F(TrustTokenKeyCommitmentControllerTest, Success) {
  TestURLLoaderFactory factory;
  factory.AddResponse(IssuerDotComKeyCommitmentPath().spec(), "", net::HTTP_OK);

  CommitmentWaiter waiter;

  auto url_request = MakeURLRequest("https://issuer.com/");

  TrustTokenKeyCommitmentController my_controller(
      waiter.Callback(), *url_request,
      url::Origin::Create(GURL("https://toplevel.com/")),
      TRAFFIC_ANNOTATION_FOR_TESTS, &factory,
      std::make_unique<FixedKeyCommitmentParser>());

  auto [result_status, result] = waiter.WaitForResult();
  EXPECT_EQ(result_status.value,
            TrustTokenKeyCommitmentController::Status::Value::kOk);
  ASSERT_TRUE(result);
  EXPECT_TRUE(mojo::Equals(
      result, FixedKeyCommitmentParser::DeterministicallyReturnedValue()));
}

}  // namespace network
