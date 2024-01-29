// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_request_helper_factory.h"

#include <optional>
#include <string_view>

#include "base/no_destructor.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "net/base/isolation_info.h"
#include "net/url_request/url_request.h"
#include "services/network/public/cpp/optional_trust_token_params.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/trust_token_http_headers.h"
#include "services/network/public/cpp/trust_token_parameterization.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "services/network/test/trust_token_test_util.h"
#include "services/network/trust_tokens/pending_trust_token_store.h"
#include "services/network/trust_tokens/trust_token_parameterization.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace network {

namespace {

using Outcome = internal::TrustTokenRequestHelperFactoryOutcome;

// These origins are not suitable for keying persistent Trust Tokens state:
// - UnsuitableUntrustworthyOrigin is not potentially trustworthy.
// - UnsuitableNonHttpNonHttpsOrigin is neither HTTP nor HTTPS.
const url::Origin& UnsuitableUntrustworthyOrigin() {
  // Default origins are opaque and, as a consequence, not potentially
  // trustworthy.
  static base::NoDestructor<url::Origin> origin{url::Origin()};
  return *origin;
}

const url::Origin& UnsuitableNonHttpNonHttpsOrigin() {
  const char kUnsuitableNonHttpNonHttpsUrl[] = "file:///";
  static base::NoDestructor<url::Origin> origin(
      url::Origin::Create(GURL(kUnsuitableNonHttpNonHttpsUrl)));
  return *origin;
}

// Creates an IsolationInfo for the given top frame origin. (We co-opt
// CreateForInternalRequest for this purpose, but define an alias because we're
// not actually creating internal requests.)
auto CreateIsolationInfo = &net::IsolationInfo::CreateForInternalRequest;

}  // namespace

class TrustTokenRequestHelperFactoryTest : public ::testing::Test {
 public:
  TrustTokenRequestHelperFactoryTest() {
    suitable_request_ = CreateSuitableRequest();
    suitable_params_ = mojom::TrustTokenParams::New();
    suitable_params_->operation = mojom::TrustTokenOperationType::kSigning;
    suitable_params_->issuers.push_back(
        url::Origin::Create(GURL("https://issuer.example")));
    store_.OnStoreReady(TrustTokenStore::CreateForTesting());
  }

 protected:
  const net::URLRequest& suitable_request() const { return *suitable_request_; }
  const mojom::TrustTokenParams& suitable_signing_params() const {
    return *suitable_params_;
  }

  std::unique_ptr<net::URLRequest> CreateSuitableRequest() {
    auto ret = maker_.MakeURLRequest("https://destination.example");
    ret->set_isolation_info(CreateIsolationInfo(
        url::Origin::Create(GURL("https://toplevel.example"))));
    return ret;
  }

  class NoopTrustTokenKeyCommitmentGetter
      : public TrustTokenKeyCommitmentGetter {
   public:
    NoopTrustTokenKeyCommitmentGetter() = default;
    void Get(const url::Origin& origin,
             base::OnceCallback<void(mojom::TrustTokenKeyCommitmentResultPtr)>
                 on_done) const override {}
  };

  TrustTokenStatusOrRequestHelper CreateHelperAndWaitForResult(
      const net::URLRequest& request,
      const mojom::TrustTokenParams& params) {
    base::RunLoop run_loop;
    TrustTokenStatusOrRequestHelper obtained_result;

    TrustTokenRequestHelperFactory(
        &store_, &getter_,
        base::BindRepeating(
            []() -> mojom::NetworkContextClient* { return nullptr; }),
        base::BindRepeating([]() { return true; }))
        .CreateTrustTokenHelperForRequest(
            request.isolation_info().top_frame_origin().value_or(url::Origin()),
            request.extra_request_headers(), params, request.net_log(),
            base::BindLambdaForTesting(
                [&](TrustTokenStatusOrRequestHelper result) {
                  obtained_result = std::move(result);
                  run_loop.Quit();
                }));

    run_loop.Run();
    return obtained_result;
  }

 private:
  base::test::TaskEnvironment env_;
  TestURLRequestMaker maker_;
  std::unique_ptr<net::URLRequest> suitable_request_;
  mojom::TrustTokenParamsPtr suitable_params_;
  PendingTrustTokenStore store_;
  NoopTrustTokenKeyCommitmentGetter getter_;
};

TEST_F(TrustTokenRequestHelperFactoryTest, MissingTopFrameOrigin) {
  base::HistogramTester histogram_tester;
  std::unique_ptr<net::URLRequest> request = CreateSuitableRequest();
  request->set_isolation_info(net::IsolationInfo());

  EXPECT_EQ(CreateHelperAndWaitForResult(*request, suitable_signing_params())
                .status(),
            mojom::TrustTokenOperationStatus::kFailedPrecondition);

  histogram_tester.ExpectUniqueSample(
      "Net.TrustTokens.RequestHelperFactoryOutcome.Signing",
      Outcome::kUnsuitableTopFrameOrigin, 1);
}

TEST_F(TrustTokenRequestHelperFactoryTest, UnsuitableTopFrameOrigin) {
  base::HistogramTester histogram_tester;
  auto request = CreateSuitableRequest();
  request->set_isolation_info(
      CreateIsolationInfo(UnsuitableUntrustworthyOrigin()));

  EXPECT_EQ(CreateHelperAndWaitForResult(*request, suitable_signing_params())
                .status(),
            mojom::TrustTokenOperationStatus::kFailedPrecondition);

  request->set_isolation_info(
      CreateIsolationInfo(UnsuitableNonHttpNonHttpsOrigin()));
  EXPECT_EQ(CreateHelperAndWaitForResult(*request, suitable_signing_params())
                .status(),
            mojom::TrustTokenOperationStatus::kFailedPrecondition);

  histogram_tester.ExpectUniqueSample(
      "Net.TrustTokens.RequestHelperFactoryOutcome.Signing",
      Outcome::kUnsuitableTopFrameOrigin, 2);
}

TEST_F(TrustTokenRequestHelperFactoryTest, ForbiddenHeaders) {
  base::HistogramTester histogram_tester;
  for (const std::string_view& header : TrustTokensRequestHeaders()) {
    std::unique_ptr<net::URLRequest> my_request = CreateSuitableRequest();
    my_request->SetExtraRequestHeaderByName(std::string(header), " ",
                                            /*overwrite=*/true);

    EXPECT_EQ(
        CreateHelperAndWaitForResult(*my_request, suitable_signing_params())
            .status(),
        mojom::TrustTokenOperationStatus::kInvalidArgument);
  }

  histogram_tester.ExpectUniqueSample(
      "Net.TrustTokens.RequestHelperFactoryOutcome.Signing",
      Outcome::kRequestRejectedDueToBearingAnInternalTrustTokensHeader,
      std::size(TrustTokensRequestHeaders()));
}

TEST_F(TrustTokenRequestHelperFactoryTest,
       CreatingSigningHelperRequiresSuitableIssuers) {
  base::HistogramTester histogram_tester;
  auto request = CreateSuitableRequest();

  auto params = suitable_signing_params().Clone();
  params->operation = mojom::TrustTokenOperationType::kSigning;
  params->issuers.clear();

  EXPECT_EQ(CreateHelperAndWaitForResult(*request, *params).status(),
            mojom::TrustTokenOperationStatus::kInvalidArgument);

  histogram_tester.ExpectUniqueSample(
      "Net.TrustTokens.RequestHelperFactoryOutcome.Signing",
      Outcome::kEmptyIssuersParameter, 1);

  params->issuers.push_back(UnsuitableUntrustworthyOrigin());
  EXPECT_EQ(CreateHelperAndWaitForResult(*request, *params).status(),
            mojom::TrustTokenOperationStatus::kInvalidArgument);

  histogram_tester.ExpectBucketCount(
      "Net.TrustTokens.RequestHelperFactoryOutcome.Signing",
      Outcome::kUnsuitableIssuerInIssuersParameter, 1);

  params->issuers.clear();
  params->issuers.push_back(UnsuitableNonHttpNonHttpsOrigin());
  EXPECT_EQ(CreateHelperAndWaitForResult(*request, *params).status(),
            mojom::TrustTokenOperationStatus::kInvalidArgument);

  histogram_tester.ExpectBucketCount(
      "Net.TrustTokens.RequestHelperFactoryOutcome.Signing",
      Outcome::kUnsuitableIssuerInIssuersParameter, 2);
}

TEST_F(TrustTokenRequestHelperFactoryTest,
       WillCreateSigningHelperWithAdditionalData) {
  auto request = CreateSuitableRequest();

  auto params = suitable_signing_params().Clone();
  params->operation = mojom::TrustTokenOperationType::kSigning;
  params->possibly_unsafe_additional_signing_data =
      std::string(kTrustTokenAdditionalSigningDataMaxSizeBytes, 'a');

  auto result = CreateHelperAndWaitForResult(suitable_request(), *params);
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result.TakeOrCrash());
}

TEST_F(TrustTokenRequestHelperFactoryTest, CreatesSigningHelper) {
  base::HistogramTester histogram_tester;
  auto params = suitable_signing_params().Clone();
  params->operation = mojom::TrustTokenOperationType::kSigning;

  auto result = CreateHelperAndWaitForResult(suitable_request(), *params);
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result.TakeOrCrash());

  histogram_tester.ExpectUniqueSample(
      "Net.TrustTokens.RequestHelperFactoryOutcome.Signing",
      Outcome::kSuccessfullyCreatedASigningHelper, 1);
}

TEST_F(TrustTokenRequestHelperFactoryTest, CreatesIssuanceHelper) {
  base::HistogramTester histogram_tester;
  auto params = suitable_signing_params().Clone();
  params->operation = mojom::TrustTokenOperationType::kIssuance;

  auto result = CreateHelperAndWaitForResult(suitable_request(), *params);
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result.TakeOrCrash());

  histogram_tester.ExpectUniqueSample(
      "Net.TrustTokens.RequestHelperFactoryOutcome.Issuance",
      Outcome::kSuccessfullyCreatedAnIssuanceHelper, 1);
}

TEST_F(TrustTokenRequestHelperFactoryTest, CreatesRedemptionHelper) {
  base::HistogramTester histogram_tester;
  auto params = suitable_signing_params().Clone();
  params->operation = mojom::TrustTokenOperationType::kRedemption;

  auto result = CreateHelperAndWaitForResult(suitable_request(), *params);
  ASSERT_TRUE(result.ok());
  EXPECT_TRUE(result.TakeOrCrash());

  histogram_tester.ExpectUniqueSample(
      "Net.TrustTokens.RequestHelperFactoryOutcome.Redemption",
      Outcome::kSuccessfullyCreatedARedemptionHelper, 1);
}

TEST_F(TrustTokenRequestHelperFactoryTest, RespectsAuthorizer) {
  base::HistogramTester histogram_tester;
  base::RunLoop run_loop;
  TrustTokenStatusOrRequestHelper obtained_result;
  PendingTrustTokenStore store;

  store.OnStoreReady(TrustTokenStore::CreateForTesting());
  NoopTrustTokenKeyCommitmentGetter getter;

  TrustTokenRequestHelperFactory(
      &store, &getter,
      base::BindRepeating(
          []() -> mojom::NetworkContextClient* { return nullptr; }),
      base::BindRepeating([]() { return false; }))
      .CreateTrustTokenHelperForRequest(
          *suitable_request().isolation_info().top_frame_origin(),
          suitable_request().extra_request_headers(), suitable_signing_params(),
          suitable_request().net_log(),
          base::BindLambdaForTesting(
              [&](TrustTokenStatusOrRequestHelper result) {
                obtained_result = std::move(result);
                run_loop.Quit();
              }));

  run_loop.Run();

  EXPECT_EQ(obtained_result.status(),
            mojom::TrustTokenOperationStatus::kUnauthorized);
  histogram_tester.ExpectUniqueSample(
      "Net.TrustTokens.RequestHelperFactoryOutcome.Signing",
      Outcome::kRejectedByAuthorizer, 1);
}
}  // namespace network
