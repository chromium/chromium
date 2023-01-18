// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_NETWORK_ATTRIBUTION_ATTRIBUTION_TEST_UTILS_H_
#define SERVICES_NETWORK_ATTRIBUTION_ATTRIBUTION_TEST_UTILS_H_

#include <set>
#include <string>

#include "base/strings/string_piece_forward.h"
#include "services/network/attribution/attribution_attestation_mediator.h"
#include "services/network/public/mojom/trust_tokens.mojom-shared.h"
#include "services/network/trust_tokens/trust_token_key_commitments.h"
#include "url/gurl.h"

namespace network {

class FakeCryptographer : public AttributionAttestationMediator::Cryptographer {
 public:
  FakeCryptographer();
  ~FakeCryptographer() override;

  bool Initialize(
      mojom::TrustTokenProtocolVersion issuer_configured_version) override;

  bool AddKey(base::StringPiece key) override;

  absl::optional<std::string> BeginIssuance(base::StringPiece message) override;

  absl::optional<std::string> ConfirmIssuanceAndBeginRedemption(
      base::StringPiece response_header) override;

  //***********************
  // Helper methods below
  //***********************

  // Returns true if `potential_blind_message` is the blind version of `message`
  static bool IsBlindMessage(const std::string& potential_blind_message,
                             const std::string& message);

  // Returns the message that was used to produce `blind_message`.
  static std::string UnblindMessage(const std::string& blind_message);

  // Returns true if `potential_token` is a token for `blind_token`.
  static bool IsToken(const std::string& potential_token,
                      const std::string& blind_token);

  mojom::TrustTokenProtocolVersion version() const { return version_; }

  void set_should_fail_initialize(bool should_fail) {
    should_fail_initialize_ = should_fail;
  }

  void set_should_fail_add_key(bool should_fail) {
    should_fail_add_key_ = should_fail;
  }

  void set_should_fail_begin_issuance(bool should_fail) {
    should_fail_begin_issuance_ = should_fail;
  }

  void set_should_fail_confirm_issuance(bool should_fail) {
    should_fail_confirm_issuance_ = should_fail;
  }

  std::set<std::string> keys;

 private:
  static constexpr char kBlindingKey[] = "blind-";
  static constexpr char kUnblindKey[] = "token-for-";

  mojom::TrustTokenProtocolVersion version_;

  bool should_fail_initialize_ = false;
  bool should_fail_add_key_ = false;
  bool should_fail_begin_issuance_ = false;
  bool should_fail_confirm_issuance_ = false;
};

std::unique_ptr<TrustTokenKeyCommitments> CreateTestTrustTokenKeyCommitments(
    std::string key,
    mojom::TrustTokenProtocolVersion protocol_version,
    GURL issuer_url);

}  // namespace network

#endif  // SERVICES_NETWORK_ATTRIBUTION_ATTRIBUTION_TEST_UTILS_H_
