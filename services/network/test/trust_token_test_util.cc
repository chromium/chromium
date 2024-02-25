// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/trust_token_test_util.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_builder.h"
#include "services/network/public/mojom/url_response_head.mojom.h"

namespace network {

TestURLRequestMaker::TestURLRequestMaker() {
  auto context_builder = net::CreateTestURLRequestContextBuilder();
  context_builder->set_net_log(net::NetLog::Get());
  context_ = context_builder->Build();
}

TestURLRequestMaker::~TestURLRequestMaker() = default;

std::unique_ptr<net::URLRequest> TestURLRequestMaker::MakeURLRequest(
    std::string_view spec) {
  return context_->CreateRequest(GURL(spec),
                                 net::RequestPriority::DEFAULT_PRIORITY,
                                 &delegate_, TRAFFIC_ANNOTATION_FOR_TESTS);
}

TrustTokenRequestHelperTest::TrustTokenRequestHelperTest(
    base::test::TaskEnvironment::TimeSource time_source)
    : env_(time_source,
           // Since the various TrustTokenRequestHelper implementations might be
           // posting tasks from within calls to Begin or Finalize, use
           // execution mode ASYNC to ensure these tasks get run during
           // RunLoop::Run calls.
           base::test::TaskEnvironment::ThreadPoolExecutionMode::ASYNC) {}
TrustTokenRequestHelperTest::~TrustTokenRequestHelperTest() = default;

mojom::TrustTokenOperationStatus
TrustTokenRequestHelperTest::ExecuteBeginOperationAndWaitForResult(
    TrustTokenRequestHelper* helper,
    net::URLRequest* request) {
  base::test::TestFuture<std::optional<net::HttpRequestHeaders>,
                         mojom::TrustTokenOperationStatus>
      future;
  helper->Begin(request->url(), future.GetCallback());
  auto [headers, status] = future.Take();
  if (headers) {
    request->SetExtraRequestHeaders(*headers);
  }
  return status;
}

mojom::TrustTokenOperationStatus
TrustTokenRequestHelperTest::ExecuteFinalizeAndWaitForResult(
    TrustTokenRequestHelper* helper,
    mojom::URLResponseHead* response) {
  base::test::TestFuture<mojom::TrustTokenOperationStatus> future;
  helper->Finalize(*response->headers.get(), future.GetCallback());
  return future.Get();
}

std::string TrustTokenEnumToString(mojom::TrustTokenOperationType operation) {
  switch (operation) {
    case mojom::TrustTokenOperationType::kIssuance:
      return "token-request";
    case mojom::TrustTokenOperationType::kRedemption:
      return "token-redemption";
    case mojom::TrustTokenOperationType::kSigning:
      return "send-redemption-record";
  }
}

std::string TrustTokenEnumToString(mojom::TrustTokenRefreshPolicy policy) {
  switch (policy) {
    case mojom::TrustTokenRefreshPolicy::kUseCached:
      return "none";
    case mojom::TrustTokenRefreshPolicy::kRefresh:
      return "refresh";
  }
}

std::string TrustTokenEnumToString(
    mojom::TrustTokenSignRequestData sign_request_data) {
  switch (sign_request_data) {
    case mojom::TrustTokenSignRequestData::kOmit:
      return "omit";
    case mojom::TrustTokenSignRequestData::kHeadersOnly:
      return "headers-only";
    case mojom::TrustTokenSignRequestData::kInclude:
      return "include";
  }
}

TrustTokenParametersAndSerialization::TrustTokenParametersAndSerialization(
    mojom::TrustTokenParamsPtr params,
    std::string serialized_params)
    : params(std::move(params)),
      serialized_params(std::move(serialized_params)) {}
TrustTokenParametersAndSerialization::~TrustTokenParametersAndSerialization() =
    default;

TrustTokenParametersAndSerialization::TrustTokenParametersAndSerialization(
    TrustTokenParametersAndSerialization&&) = default;
TrustTokenParametersAndSerialization&
TrustTokenParametersAndSerialization::operator=(
    TrustTokenParametersAndSerialization&&) = default;

TrustTokenTestParameters::~TrustTokenTestParameters() = default;
TrustTokenTestParameters::TrustTokenTestParameters(
    const TrustTokenTestParameters&) = default;
TrustTokenTestParameters& TrustTokenTestParameters::operator=(
    const TrustTokenTestParameters&) = default;

TrustTokenTestParameters::TrustTokenTestParameters(
    int version,
    network::mojom::TrustTokenOperationType operation,
    std::optional<network::mojom::TrustTokenRefreshPolicy> refresh_policy,
    std::optional<std::vector<std::string>> issuer_specs)
    : version(version),
      operation(operation),
      refresh_policy(refresh_policy),
      issuer_specs(issuer_specs) {}

TrustTokenParametersAndSerialization
SerializeTrustTokenParametersAndConstructExpectation(
    const TrustTokenTestParameters& input) {
  auto trust_token_params = mojom::TrustTokenParams::New();

  auto parameters =
      base::Value::Dict()
          .Set("version", input.version)
          .Set("operation", TrustTokenEnumToString(input.operation));
  trust_token_params->operation = input.operation;

  if (input.refresh_policy.has_value()) {
    parameters.Set("refreshPolicy",
                   TrustTokenEnumToString(*input.refresh_policy));
    trust_token_params->refresh_policy = *input.refresh_policy;
  }

  if (input.issuer_specs.has_value()) {
    base::Value::List issuers;
    for (const std::string& issuer_spec : *input.issuer_specs) {
      issuers.Append(issuer_spec);
      trust_token_params->issuers.push_back(
          url::Origin::Create(GURL(issuer_spec)));
    }
    parameters.Set("issuers", std::move(issuers));
  }

  return {std::move(trust_token_params), *base::WriteJson(parameters)};
}

std::string WrapKeyCommitmentsForIssuers(
    base::flat_map<url::Origin, std::string_view> issuers_and_commitments) {
  base::Value::Dict to_serialize;
  for (const auto& [issuer, commitment] : issuers_and_commitments) {
    // guard against accidentally passing an origin without a unique
    // serialization
    CHECK_NE(issuer.Serialize(), "null");

    to_serialize.Set(issuer.Serialize(), *base::JSONReader::Read(commitment));
  }
  return *base::WriteJson(to_serialize);
}

}  // namespace network
