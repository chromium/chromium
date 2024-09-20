// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/test/trust_token_request_handler.h"

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/json/json_string_value_serializer.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "crypto/sha2.h"
#include "net/http/http_request_headers.h"
#include "net/http/structured_headers.h"
#include "services/network/public/cpp/trust_token_http_headers.h"
#include "services/network/trust_tokens/scoped_boringssl_bytes.h"
#include "services/network/trust_tokens/types.h"
#include "third_party/boringssl/src/include/openssl/curve25519.h"
#include "third_party/boringssl/src/include/openssl/evp.h"
#include "third_party/boringssl/src/include/openssl/trust_token.h"

namespace network::test {
namespace {

struct IssuanceKeyPair {
  // Token signing and verification keys:
  std::vector<uint8_t> signing;
  std::vector<uint8_t> verification;

  // Default to a very long expiry time, but allow this to be overridden when
  // specific tests want to do so.
  base::Time expiry = base::Time::Max();
};

IssuanceKeyPair GenerateIssuanceKeyPair(int id) {
  IssuanceKeyPair keys;
  keys.signing.resize(TRUST_TOKEN_MAX_PRIVATE_KEY_SIZE);
  keys.verification.resize(TRUST_TOKEN_MAX_PUBLIC_KEY_SIZE);
  size_t signing_key_len, verification_key_len;
  CHECK(TRUST_TOKEN_generate_key(
      TRUST_TOKEN_experiment_v2_pmb(), keys.signing.data(), &signing_key_len,
      keys.signing.size(), keys.verification.data(), &verification_key_len,
      keys.verification.size(), id));
  keys.signing.resize(signing_key_len);
  keys.verification.resize(verification_key_len);

  return keys;
}

// This convenience helper prevents forgetting whether the inequality is weak or
// strict.
bool HasKeyPairExpired(const IssuanceKeyPair& p) {
  return p.expiry <= base::Time::Now();
}

}  // namespace

TrustTokenRequestHandler::Options::Options() = default;
TrustTokenRequestHandler::Options::~Options() = default;
TrustTokenRequestHandler::Options::Options(const Options&) = default;
TrustTokenRequestHandler::Options& TrustTokenRequestHandler::Options::operator=(
    const Options&) = default;

struct TrustTokenRequestHandler::Rep {
  // The protocol version to use.
  std::string protocol_version;

  // The commitment ID to use.
  int id;

  // Issue at most this many tokens per issuance.
  int batch_size;

  std::vector<IssuanceKeyPair> issuance_keys;

  // Whether to peremptorily reject issuance and redemption or whether to
  // actually process the provided input.
  ServerOperationOutcome issuance_outcome;
  ServerOperationOutcome redemption_outcome;

  // Creates a BoringSSL token issuer context suitable for issuance or
  // redemption, using only the unexpired key pairs from |issuance_keys|.
  bssl::UniquePtr<TRUST_TOKEN_ISSUER> CreateIssuerContextFromUnexpiredKeys()
      const;

  // This is a structured representation of the most recent input to
  // RecordSignedRequest.
  std::optional<TrustTokenSignedRequest> last_incoming_signed_request;
};

bssl::UniquePtr<TRUST_TOKEN_ISSUER>
TrustTokenRequestHandler::Rep::CreateIssuerContextFromUnexpiredKeys() const {
  bssl::UniquePtr<TRUST_TOKEN_ISSUER> ret(
      TRUST_TOKEN_ISSUER_new(TRUST_TOKEN_experiment_v2_pmb(), batch_size));
  if (!ret) {
    return nullptr;
  }

  for (const IssuanceKeyPair& key_pair : issuance_keys) {
    if (HasKeyPairExpired(key_pair)) {
      continue;
    }

    if (!TRUST_TOKEN_ISSUER_add_key(ret.get(), key_pair.signing.data(),
                                    key_pair.signing.size())) {
      return nullptr;
    }
  }

  // Copying the comment from evp.h:
  // The [Ed25519] RFC 8032 private key format is the 32-byte prefix of
  // |ED25519_sign|'s 64-byte private key.
  uint8_t public_key[32], private_key[64];
  ED25519_keypair(public_key, private_key);
  bssl::UniquePtr<EVP_PKEY> issuer_rr_key(EVP_PKEY_new_raw_private_key(
      EVP_PKEY_ED25519, /*unused=*/nullptr, private_key,
      /*len=*/32));

  if (!issuer_rr_key) {
    return nullptr;
  }

  if (!TRUST_TOKEN_ISSUER_set_srr_key(ret.get(), issuer_rr_key.get())) {
    return nullptr;
  }

  return ret;
}

TrustTokenRequestHandler::TrustTokenRequestHandler(Options options) {
  UpdateOptions(std::move(options));
}

TrustTokenRequestHandler::TrustTokenRequestHandler()
    : TrustTokenRequestHandler(Options()) {}

TrustTokenRequestHandler::~TrustTokenRequestHandler() = default;

std::string TrustTokenRequestHandler::GetKeyCommitmentRecord() const {
  base::AutoLock lock(mutex_);

  std::string ret;
  JSONStringValueSerializer serializer(&ret);

  base::Value::Dict dict;
  const std::string protocol_string = internal::ProtocolVersionToString(
      mojom::TrustTokenProtocolVersion::kTrustTokenV3Pmb);
  dict.SetByDottedPath(protocol_string + ".protocol_version",
                       rep_->protocol_version);
  dict.SetByDottedPath(protocol_string + ".id", rep_->id);
  dict.SetByDottedPath(protocol_string + ".batchsize", rep_->batch_size);

  for (size_t i = 0; i < rep_->issuance_keys.size(); ++i) {
    dict.SetByDottedPath(
        protocol_string + ".keys." + base::NumberToString(i) + ".Y",
        base::Base64Encode(
            base::make_span(rep_->issuance_keys[i].verification)));
    dict.SetByDottedPath(
        protocol_string + ".keys." + base::NumberToString(i) + ".expiry",
        base::NumberToString(
            (rep_->issuance_keys[i].expiry - base::Time::UnixEpoch())
                .InMicroseconds()));
  }

  // It's OK to be a bit crashy in exceptional failure cases because it
  // indicates a serious coding error in this test-only code; we'd like to find
  // this out sooner rather than later.
  CHECK(serializer.Serialize(dict));
  return ret;
}

std::optional<std::string> TrustTokenRequestHandler::Issue(
    std::string_view issuance_request) {
  base::AutoLock lock(mutex_);

  if (rep_->issuance_outcome == ServerOperationOutcome::kUnconditionalFailure) {
    return std::nullopt;
  }

  bssl::UniquePtr<TRUST_TOKEN_ISSUER> issuer_ctx =
      rep_->CreateIssuerContextFromUnexpiredKeys();

  std::string decoded_issuance_request;
  if (!base::Base64Decode(issuance_request, &decoded_issuance_request)) {
    return std::nullopt;
  }

  // TODO(davidvc): Perhaps make this configurable? Not a high priority, though.
  constexpr uint8_t kPrivateMetadata = 0;

  ScopedBoringsslBytes decoded_issuance_response;
  size_t num_tokens_issued = 0;
  bool ok = false;

  for (size_t i = 0; i < rep_->issuance_keys.size(); ++i) {
    if (HasKeyPairExpired(rep_->issuance_keys[i])) {
      continue;
    }

    if (TRUST_TOKEN_ISSUER_issue(
            issuer_ctx.get(),
            &decoded_issuance_response.mutable_ptr()->AsEphemeralRawAddr(),
            decoded_issuance_response.mutable_len(), &num_tokens_issued,
            base::as_bytes(base::make_span(decoded_issuance_request)).data(),
            decoded_issuance_request.size(),
            /*public_metadata=*/static_cast<uint32_t>(i), kPrivateMetadata,
            rep_->batch_size)) {
      ok = true;
      break;
    }
  }

  if (!ok) {
    return std::nullopt;
  }

  return base::Base64Encode(decoded_issuance_response.as_span());
}

std::optional<std::string> TrustTokenRequestHandler::Redeem(
    std::string_view redemption_request) {
  base::AutoLock lock(mutex_);

  if (rep_->redemption_outcome ==
      ServerOperationOutcome::kUnconditionalFailure) {
    return std::nullopt;
  }

  bssl::UniquePtr<TRUST_TOKEN_ISSUER> issuer_ctx =
      rep_->CreateIssuerContextFromUnexpiredKeys();

  std::string decoded_redemption_request;
  if (!base::Base64Decode(redemption_request, &decoded_redemption_request)) {
    return std::nullopt;
  }

  TRUST_TOKEN* redeemed_token;
  ScopedBoringsslBytes redeemed_client_data;
  uint32_t received_public_metadata;
  uint8_t received_private_metadata;
  if (!TRUST_TOKEN_ISSUER_redeem(
          issuer_ctx.get(), &received_public_metadata,
          &received_private_metadata, &redeemed_token,
          &redeemed_client_data.mutable_ptr()->AsEphemeralRawAddr(),
          redeemed_client_data.mutable_len(),
          base::as_bytes(base::make_span(decoded_redemption_request)).data(),
          decoded_redemption_request.size())) {
    return std::nullopt;
  }

  // Put the issuer-receied token in a smart pointer so it will get deleted on
  // leaving scope.
  bssl::UniquePtr<TRUST_TOKEN> redeemed_token_scoper(redeemed_token);

  return base::Base64Encode(base::as_bytes(base::make_span(base::StringPrintf(
      "%d:%d", received_public_metadata, received_private_metadata))));
}

void TrustTokenRequestHandler::RecordSignedRequest(
    const GURL& destination,
    const net::HttpRequestHeaders& headers) {
  base::AutoLock lock(mutex_);

  rep_->last_incoming_signed_request =
      TrustTokenSignedRequest{destination, headers};
}

std::optional<TrustTokenSignedRequest>
TrustTokenRequestHandler::last_incoming_signed_request() const {
  base::AutoLock lock(mutex_);
  return rep_->last_incoming_signed_request;
}

void TrustTokenRequestHandler::UpdateOptions(Options options) {
  base::AutoLock lock(mutex_);

  rep_ = std::make_unique<Rep>();

  rep_->protocol_version = options.protocol_version;
  rep_->id = options.id;
  rep_->batch_size = options.batch_size;
  rep_->issuance_outcome = options.issuance_outcome;
  rep_->redemption_outcome = options.redemption_outcome;

  for (int i = 0; i < options.num_keys; ++i) {
    rep_->issuance_keys.push_back(GenerateIssuanceKeyPair(i));
  }
}

}  // namespace network::test
