// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/attribution/attribution_test_utils.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "base/ranges/algorithm.h"
#include "base/strings/strcat.h"
#include "base/strings/string_piece_forward.h"
#include "base/strings/string_util.h"
#include "net/http/structured_headers.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/attribution/attribution_request_helper.h"
#include "services/network/attribution/attribution_verification_mediator.h"
#include "services/network/attribution/attribution_verification_mediator_metrics_recorder.h"
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

std::unique_ptr<net::test_server::HttpResponse> HandleVerificationRequest(
    const net::test_server::HttpRequest& request) {
  if (!base::StartsWith(request.relative_url, kVerificationHandlerPathPrefix)) {
    return nullptr;
  }

  auto verification_header =
      base::ranges::find_if(request.headers, [](auto& s) {
        return s.first ==
               AttributionVerificationMediator::kReportVerificationHeader;
      });
  bool verification_header_set =
      verification_header != std::end(request.headers);
  EXPECT_TRUE(verification_header_set);
  if (!verification_header_set) {
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

  size_t received_blind_messages_count =
      DeserializeStructuredHeaderListOfStrings(verification_header->second)
          .size();

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->AddCustomHeader(
      AttributionVerificationMediator::kReportVerificationHeader,
      SerializeStructureHeaderListOfStrings(std::vector<std::string>(
          received_blind_messages_count, kTestBlindToken)));

  if (request.relative_url == kRedirectVerificationRequestPath) {
    http_response->set_code(net::HTTP_FOUND);
    http_response->AddCustomHeader(
        "Location",
        base::JoinString(
            {kVerificationHandlerPathPrefix, "some-different-path"}, "/"));
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
      /*create_mediator=*/base::BindRepeating(&CreateTestVerificationMediator,
                                              trust_token_key_commitments));
}

AttributionVerificationMediator CreateTestVerificationMediator(
    TrustTokenKeyCommitments* trust_token_key_commitments) {
  std::vector<std::unique_ptr<AttributionVerificationMediator::Cryptographer>>
      cryptographers;
  cryptographers.reserve(
      AttributionRequestHelper::kVerificationTokensPerTrigger);
  for (size_t i = 0;
       i < AttributionRequestHelper::kVerificationTokensPerTrigger; ++i) {
    cryptographers.push_back(std::make_unique<FakeCryptographer>());
  }
  return AttributionVerificationMediator(
      trust_token_key_commitments, std::move(cryptographers),
      std::make_unique<AttributionVerificationMediatorMetricsRecorder>());
}

std::vector<const std::string> DeserializeStructuredHeaderListOfStrings(
    base::StringPiece header) {
  absl::optional<net::structured_headers::List> parsed_list =
      net::structured_headers::ParseList(header);
  if (!parsed_list.has_value()) {
    return {};
  }

  std::vector<const std::string> strings;
  strings.reserve(parsed_list->size());
  for (const auto& item : parsed_list.value()) {
    if (item.member_is_inner_list || item.member.size() != 1u ||
        !item.member.at(0).item.is_string()) {
      return {};
    }
    strings.emplace_back(item.member.at(0).item.GetString());
  }

  return strings;
}

std::string SerializeStructureHeaderListOfStrings(
    const std::vector<std::string>& strings) {
  net::structured_headers::List headers;

  for (const std::string& string : strings) {
    net::structured_headers::Item item(
        string, net::structured_headers::Item::ItemType::kStringType);
    headers.emplace_back(
        net::structured_headers::ParameterizedMember(item, {}));
  }
  absl::optional<std::string> serialized =
      net::structured_headers::SerializeList(headers);
  CHECK(serialized.has_value());
  return serialized.value();
}

}  // namespace network
