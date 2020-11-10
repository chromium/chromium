// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/test/trust_token_test_util.h"

#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/values.h"

namespace network {

TestURLRequestMaker::TestURLRequestMaker() {
  context_.set_net_log(&net_log_);
}
TestURLRequestMaker::~TestURLRequestMaker() = default;

std::unique_ptr<net::URLRequest> TestURLRequestMaker::MakeURLRequest(
    base::StringPiece spec) {
  return context_.CreateRequest(GURL(spec),
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
  base::RunLoop run_loop;
  mojom::TrustTokenOperationStatus status;
  helper->Begin(request,
                base::BindLambdaForTesting(
                    [&](mojom::TrustTokenOperationStatus returned_status) {
                      status = returned_status;
                      run_loop.Quit();
                    }));
  run_loop.Run();
  return status;
}

mojom::TrustTokenOperationStatus
TrustTokenRequestHelperTest::ExecuteFinalizeAndWaitForResult(
    TrustTokenRequestHelper* helper,
    mojom::URLResponseHead* response) {
  base::RunLoop run_loop;
  mojom::TrustTokenOperationStatus status;
  helper->Finalize(response,
                   base::BindLambdaForTesting(
                       [&](mojom::TrustTokenOperationStatus returned_status) {
                         status = returned_status;
                         run_loop.Quit();
                       }));
  run_loop.Run();
  return status;
}

std::string TrustTokenEnumToString(mojom::TrustTokenOperationType type) {
  switch (type) {
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
    network::mojom::TrustTokenOperationType type,
    base::Optional<network::mojom::TrustTokenRefreshPolicy> refresh_policy,
    base::Optional<network::mojom::TrustTokenSignRequestData> sign_request_data,
    base::Optional<bool> include_timestamp_header,
    base::Optional<std::vector<std::string>> issuer_specs,
    base::Optional<std::vector<std::string>> additional_signed_headers,
    base::Optional<std::string> possibly_unsafe_additional_signing_data)
    : type(type),
      refresh_policy(refresh_policy),
      sign_request_data(sign_request_data),
      include_timestamp_header(include_timestamp_header),
      issuer_specs(issuer_specs),
      additional_signed_headers(additional_signed_headers),
      possibly_unsafe_additional_signing_data(
          possibly_unsafe_additional_signing_data) {}

TrustTokenParametersAndSerialization
SerializeTrustTokenParametersAndConstructExpectation(
    const TrustTokenTestParameters& input) {
  auto trust_token_params = mojom::TrustTokenParams::New();

  base::Value parameters(base::Value::Type::DICTIONARY);
  parameters.SetStringKey("type", TrustTokenEnumToString(input.type));
  trust_token_params->type = input.type;

  if (input.refresh_policy.has_value()) {
    parameters.SetStringKey("refreshPolicy",
                            TrustTokenEnumToString(*input.refresh_policy));
    trust_token_params->refresh_policy = *input.refresh_policy;
  }

  if (input.sign_request_data.has_value()) {
    parameters.SetStringKey("signRequestData",
                            TrustTokenEnumToString(*input.sign_request_data));
    trust_token_params->sign_request_data = *input.sign_request_data;
  }

  if (input.include_timestamp_header.has_value()) {
    parameters.SetBoolKey("includeTimestampHeader",
                          *input.include_timestamp_header);
    trust_token_params->include_timestamp_header =
        *input.include_timestamp_header;
  }

  if (input.issuer_specs.has_value()) {
    base::Value issuers(base::Value::Type::LIST);
    for (const std::string& issuer_spec : *input.issuer_specs) {
      issuers.Append(issuer_spec);
      trust_token_params->issuers.push_back(
          url::Origin::Create(GURL(issuer_spec)));
    }
    parameters.SetKey("issuers", std::move(issuers));
  }

  if (input.additional_signed_headers.has_value()) {
    base::Value headers(base::Value::Type::LIST);
    for (const std::string& header : *input.additional_signed_headers)
      headers.Append(header);
    parameters.SetKey("additionalSignedHeaders", std::move(headers));
    for (const std::string& input_header : *input.additional_signed_headers) {
      trust_token_params->additional_signed_headers.push_back(input_header);
    }
  }

  if (input.possibly_unsafe_additional_signing_data.has_value()) {
    parameters.SetStringKey("additionalSigningData",
                            *input.possibly_unsafe_additional_signing_data);
    trust_token_params->possibly_unsafe_additional_signing_data =
        *input.possibly_unsafe_additional_signing_data;
  }

  std::string serialized_parameters;
  JSONStringValueSerializer serializer(&serialized_parameters);
  CHECK(serializer.Serialize(parameters));

  return {std::move(trust_token_params), std::move(serialized_parameters)};
}

std::string WrapKeyCommitmentsForIssuers(
    base::flat_map<url::Origin, base::StringPiece> issuers_and_commitments) {
  std::string ret;
  JSONStringValueSerializer serializer(&ret);

  base::Value to_serialize(base::Value::Type::DICTIONARY);

  for (const auto& kv : issuers_and_commitments) {
    const url::Origin& issuer = kv.first;
    base::StringPiece commitment = kv.second;

    // guard against accidentally passing an origin without a unique
    // serialization
    CHECK_NE(issuer.Serialize(), "null");

    to_serialize.SetKey(issuer.Serialize(),
                        *base::JSONReader::Read(commitment));
  }
  CHECK(serializer.Serialize(to_serialize));
  return ret;
}

}  // namespace network
