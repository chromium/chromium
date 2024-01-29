// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_key_commitments.h"

#include <optional>
#include <utility>

#include "base/command_line.h"
#include "base/values.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/trust_tokens.mojom-forward.h"
#include "services/network/trust_tokens/suitable_trust_token_origin.h"
#include "services/network/trust_tokens/trust_token_key_commitment_parser.h"
#include "services/network/trust_tokens/trust_token_key_filtering.h"
#include "services/network/trust_tokens/trust_token_parameterization.h"

namespace network {

namespace {

base::flat_map<SuitableTrustTokenOrigin,
               mojom::TrustTokenKeyCommitmentResultPtr>
ParseCommitmentsFromCommandLine() {
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAdditionalTrustTokenKeyCommitments)) {
    return {};
  }

  std::string raw_commitments =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          network::switches::kAdditionalTrustTokenKeyCommitments);

  if (auto parsed = TrustTokenKeyCommitmentParser().ParseMultipleIssuers(
          raw_commitments)) {
    return std::move(*parsed);
  } else {
    // Complain loudly here because the user presumably only provides key
    // commitments through the command line out of a desire to _use_ the key
    // commitments.
    LOG(ERROR)
        << "Couldn't parse Trust Tokens key commitments from the command line: "
        << raw_commitments;
  }
  return {};
}

// Filters |result->keys| to contain only a small number of
// soon-to-expire-but-not-yet-expired keys, then passes |result| to |done|.
mojom::TrustTokenKeyCommitmentResultPtr FilterCommitments(
    mojom::TrustTokenKeyCommitmentResultPtr result) {
  if (result) {
    size_t max_keys = TrustTokenMaxKeysForVersion(result->protocol_version);
    RetainSoonestToExpireTrustTokenKeys(&result->keys, max_keys);
  }

  return result;
}

}  // namespace

TrustTokenKeyCommitments::TrustTokenKeyCommitments()
    : additional_commitments_from_command_line_(
          ParseCommitmentsFromCommandLine()) {}

TrustTokenKeyCommitments::~TrustTokenKeyCommitments() = default;

void TrustTokenKeyCommitments::Set(
    base::flat_map<url::Origin, mojom::TrustTokenKeyCommitmentResultPtr> map) {
  // To filter out the unsuitable origins in linear time, extract |map|'s
  // contents a vector, filter the vector, and place the result back into
  // |map_|.

  std::vector<std::pair<url::Origin, mojom::TrustTokenKeyCommitmentResultPtr>>
      to_filter(std::move(map).extract());

  std::vector<std::pair<SuitableTrustTokenOrigin,
                        mojom::TrustTokenKeyCommitmentResultPtr>>
      filtered;

  // Due to the characteristics of the Trust Tokens protocol, it is expected
  // that there be no more than a couple hundred issuer origins.
  for (std::pair<url::Origin, mojom::TrustTokenKeyCommitmentResultPtr>& kv :
       to_filter) {
    auto maybe_suitable_origin =
        SuitableTrustTokenOrigin::Create(std::move(kv.first));
    if (!maybe_suitable_origin)
      continue;
    filtered.emplace_back(std::move(*maybe_suitable_origin),
                          std::move(kv.second));
  }

  commitments_.replace(std::move(filtered));
}

void TrustTokenKeyCommitments::ParseAndSet(std::string_view raw_commitments) {
  TrustTokenKeyCommitmentParser parser;
  if (auto parsed = parser.ParseMultipleIssuers(raw_commitments))
    commitments_.swap(*parsed);
}

void TrustTokenKeyCommitments::Get(
    const url::Origin& origin,
    base::OnceCallback<void(mojom::TrustTokenKeyCommitmentResultPtr)> done)
    const {
  std::move(done).Run(GetSync(origin));
}

mojom::TrustTokenKeyCommitmentResultPtr TrustTokenKeyCommitments::GetSync(
    const url::Origin& origin) const {
  std::optional<SuitableTrustTokenOrigin> suitable_origin =
      SuitableTrustTokenOrigin::Create(origin);
  if (!suitable_origin) {
    return nullptr;
  }

  if (!additional_commitments_from_command_line_.empty()) {
    auto it = additional_commitments_from_command_line_.find(*suitable_origin);
    if (it != additional_commitments_from_command_line_.end()) {
      return FilterCommitments(it->second->Clone());
    }
  }

  auto it = commitments_.find(*suitable_origin);
  if (it == commitments_.end()) {
    return nullptr;
  }

  return FilterCommitments(it->second->Clone());
}

}  // namespace network
