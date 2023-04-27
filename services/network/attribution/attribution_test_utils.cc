// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/attribution/attribution_test_utils.h"

#include <memory>
#include <string>

#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/string_util.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/attribution/attribution_attestation_mediator.h"
#include "services/network/attribution/attribution_attestation_mediator_metrics_recorder.h"
#include "services/network/attribution/attribution_request_helper.h"
#include "services/network/public/cpp/trust_token_http_headers.h"
#include "services/network/public/mojom/attribution.mojom.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "services/network/trust_tokens/trust_token_key_commitments.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace network {

FakeCryptographer::FakeCryptographer() = default;
FakeCryptographer::~FakeCryptographer() = default;

bool FakeCryptographer::Initialize(
    mojom::TrustTokenProtocolVersion issuer_configured_version) {
  if (should_fail_initialize_) {
    return false;
  }

  version_ = issuer_configured_version;
  return true;
}

bool FakeCryptographer::AddKey(base::StringPiece key) {
  if (should_fail_add_key_) {
    return false;
  }

  keys.insert(std::string(key));
  return true;
}

absl::optional<std::string> FakeCryptographer::BeginIssuance(
    base::StringPiece message) {
  if (should_fail_begin_issuance_) {
    return absl::nullopt;
  }
  return base::StrCat({kBlindingKey, message});
}

bool FakeCryptographer::IsBlindMessage(
    const std::string& potential_blind_message,
    const std::string& message) {
  return potential_blind_message == base::StrCat({kBlindingKey, message});
}

std::string FakeCryptographer::UnblindMessage(
    const std::string& blind_message) {
  return blind_message.substr(sizeof(kBlindingKey) - 1, std::string::npos);
}

absl::optional<std::string>
FakeCryptographer::ConfirmIssuanceAndBeginRedemption(
    base::StringPiece blind_token) {
  if (should_fail_confirm_issuance_) {
    return absl::nullopt;
  }
  return base::StrCat({kUnblindKey, blind_token});
}

bool FakeCryptographer::IsToken(const std::string& potential_token,
                                const std::string& blind_token) {
  return potential_token == base::StrCat({kUnblindKey, blind_token});
}

std::unique_ptr<net::test_server::HttpResponse> HandleAttestationRequest(
    const net::test_server::HttpRequest& request) {
  if (!base::StartsWith(request.relative_url, kAttestationHandlerPathPrefix)) {
    return nullptr;
  }

  auto attestation_header = base::ranges::find_if(request.headers, [](auto& s) {
    return s.first == AttributionAttestationMediator::kTriggerAttestationHeader;
  });
  bool attestation_header_set = attestation_header != std::end(request.headers);
  EXPECT_TRUE(attestation_header_set);
  if (!attestation_header_set) {
    return nullptr;
  }

  auto trust_token_version =
      base::ranges::find_if(request.headers, [](auto& s) {
        return s.first == kTrustTokensSecTrustTokenVersionHeader;
      });
  bool trust_token_version_set =
      trust_token_version != std::end(request.headers);
  EXPECT_TRUE(trust_token_version_set);
  if (!trust_token_version_set) {
    return nullptr;
  }

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->AddCustomHeader(
      AttributionAttestationMediator::kTriggerAttestationHeader,
      kTestBlindToken);

  if (request.relative_url == kRedirectAttestationRequestPath) {
    http_response->set_code(net::HTTP_FOUND);
    http_response->AddCustomHeader(
        "Location",
        base::JoinString({kAttestationHandlerPathPrefix, "some-different-path"},
                         "/"));
  } else {
    http_response->set_code(net::HTTP_OK);
  }

  return http_response;
}

std::unique_ptr<TrustTokenKeyCommitments> CreateTestTrustTokenKeyCommitments(
    std::string key,
    mojom::TrustTokenProtocolVersion protocol_version,
    GURL issuer_url) {
  auto key_commitment_getter = std::make_unique<TrustTokenKeyCommitments>();

  auto key_commitment = mojom::TrustTokenKeyCommitmentResult::New();
  key_commitment->id = 1;
  key_commitment->keys.push_back(
      mojom::TrustTokenVerificationKey::New(key, /*expiry=*/base::Time::Max()));
  key_commitment->batch_size = 10;
  key_commitment->protocol_version = protocol_version;

  base::flat_map<url::Origin, mojom::TrustTokenKeyCommitmentResultPtr> map;
  map[SuitableTrustTokenOrigin::Create(issuer_url).value()] =
      std::move(key_commitment);
  key_commitment_getter->Set(std::move(map));

  return key_commitment_getter;
}

std::unique_ptr<AttributionRequestHelper> CreateTestAttributionRequestHelper(
    TrustTokenKeyCommitments* trust_token_key_commitments) {
  DCHECK(trust_token_key_commitments);
  return AttributionRequestHelper::CreateForTesting(
      mojom::AttributionReportingEligibility::kTrigger,
      /*create_mediator=*/base::BindRepeating(&CreateTestAttestationMediator,
                                              trust_token_key_commitments));
}

AttributionAttestationMediator CreateTestAttestationMediator(
    TrustTokenKeyCommitments* trust_token_key_commitments) {
  return AttributionAttestationMediator(
      trust_token_key_commitments, std::make_unique<FakeCryptographer>(),
      std::make_unique<AttributionAttestationMediatorMetricsRecorder>());
}

}  // namespace network
