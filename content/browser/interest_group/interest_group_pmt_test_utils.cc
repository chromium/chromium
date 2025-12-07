// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_pmt_test_utils.h"

#include <optional>
#include <string>
#include <string_view>

#include "base/base64.h"
#include "base/check.h"
#include "base/containers/contains.h"
#include "base/containers/span.h"
#include "base/strings/to_string.h"
#include "components/cbor/reader.h"
#include "components/cbor/writer.h"
#include "content/browser/interest_group/bidding_and_auction_server_key_fetcher.h"
#include "content/browser/interest_group/interest_group_pmt_report_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/hpke.h"

namespace content::PrivateModelTrainingTestUtils {
namespace {

// Helper function needed in order to test encryption.
// Returns empty vector in the case of an error.
std::optional<std::vector<uint8_t>> DecryptPayloadWithHpke(
    base::span<const uint8_t> payload,
    const EVP_HPKE_KEY& key,
    base::span<const uint8_t> expected_serialized_shared_info) {
  base::span<const uint8_t> enc = payload.first<X25519_PUBLIC_VALUE_LEN>();

  bssl::ScopedEVP_HPKE_CTX recipient_context;
  if (!EVP_HPKE_CTX_setup_recipient(
          /*ctx=*/recipient_context.get(), /*key=*/&key,
          /*kdf=*/EVP_hpke_hkdf_sha256(),
          /*aead=*/EVP_hpke_chacha20_poly1305(),
          /*enc=*/enc.data(), /*enc_len=*/enc.size(),
          /*info=*/expected_serialized_shared_info.data(),
          /*info_len=*/expected_serialized_shared_info.size())) {
    return std::nullopt;
  }

  auto ciphertext = payload.subspan<X25519_PUBLIC_VALUE_LEN>();
  std::vector<uint8_t> plaintext(ciphertext.size());
  size_t plaintext_len;

  if (!EVP_HPKE_CTX_open(
          /*ctx=*/recipient_context.get(), /*out=*/plaintext.data(),
          /*out_len*/ &plaintext_len, /*max_out_len=*/plaintext.size(),
          /*in=*/ciphertext.data(), /*in_len=*/ciphertext.size(),
          /*ad=*/nullptr,
          /*ad_len=*/0)) {
    return std::nullopt;
  }

  plaintext.resize(plaintext_len);
  return plaintext;
}

// Helper function to create the shared info CBOR with the domain separation
// prefix, meant for encryption.
std::optional<std::vector<uint8_t>> CreateSharedInfoForEncryption(
    const std::vector<uint8_t>& cbor_data) {
  // Read the CBOR data
  const auto maybe_map = cbor::Reader::Read(cbor_data);
  if (!maybe_map || !maybe_map->is_map()) {
    return std::nullopt;  // Return on CBOR read error
  }
  const auto& map = maybe_map->GetMap();

  // Extract shared_info's cbor map
  const auto shared_info_it = map.find(cbor::Value("shared_info"));
  if (shared_info_it == map.end() || !shared_info_it->second.is_map()) {
    return std::nullopt;  // Return if shared_info map is not found
  }
  const auto& shared_info_map = shared_info_it->second.GetMap();

  // Write the shared info map to CBOR
  std::optional<std::vector<uint8_t>> cbor_data_to_prefix =
      cbor::Writer::Write(cbor::Value(shared_info_map));

  if (!cbor_data_to_prefix.has_value()) {
    return std::nullopt;  // Return on CBOR write error
  }

  // Prefix the shared_info with the domain separation prefix
  std::vector<uint8_t> prefixed_cbor;
  prefixed_cbor.reserve(
      PrivateModelTrainingRequest::kDomainSeparationPrefix.size() +
      cbor_data_to_prefix.value().size());

  prefixed_cbor.insert(
      prefixed_cbor.end(),
      PrivateModelTrainingRequest::kDomainSeparationPrefix.begin(),
      PrivateModelTrainingRequest::kDomainSeparationPrefix.end());

  prefixed_cbor.insert(prefixed_cbor.end(), cbor_data_to_prefix.value().begin(),
                       cbor_data_to_prefix.value().end());

  return prefixed_cbor;
}

// Helper function to extract encrypted payload from CBOR data.
std::optional<std::vector<uint8_t>> ExtractPayloadFromCbor(
    const std::vector<uint8_t>& cbor_data) {
  const auto maybe_map = cbor::Reader::Read(cbor_data);
  if (!maybe_map || !maybe_map->is_map()) {
    return std::nullopt;
  }
  const auto& map = maybe_map->GetMap();

  const auto aggregation_service_payload_it =
      map.find(cbor::Value("aggregation_service_payload"));
  if (aggregation_service_payload_it == map.end() ||
      !aggregation_service_payload_it->second.is_map()) {
    return std::nullopt;
  }
  const auto& aggregation_service_payload_map =
      aggregation_service_payload_it->second.GetMap();

  const auto payload_it =
      aggregation_service_payload_map.find(cbor::Value("payload"));
  if (payload_it == aggregation_service_payload_map.end() ||
      !payload_it->second.is_bytestring()) {
    return std::nullopt;
  }
  return payload_it->second.GetBytestring();
}
}  // namespace

TestHpkeKey::TestHpkeKey(std::string key_id) : key_id_(std::move(key_id)) {
  EVP_HPKE_KEY_generate(full_hpke_key_.get(), EVP_hpke_x25519_hkdf_sha256());
}

TestHpkeKey::~TestHpkeKey() = default;

TestHpkeKey::TestHpkeKey(TestHpkeKey&&) = default;

TestHpkeKey& TestHpkeKey::operator=(TestHpkeKey&&) = default;

BiddingAndAuctionServerKey TestHpkeKey::GetPublicKey() const {
  std::vector<uint8_t> public_key(X25519_PUBLIC_VALUE_LEN);
  size_t public_key_len;
  EXPECT_TRUE(EVP_HPKE_KEY_public_key(
      /*key=*/full_hpke_key_.get(), /*out=*/public_key.data(),
      /*out_len=*/&public_key_len, /*max_out=*/public_key.size()));
  EXPECT_EQ(public_key.size(), public_key_len);
  return BiddingAndAuctionServerKey(
      std::string(public_key.begin(), public_key.end()), key_id_);
}

std::optional<std::vector<uint8_t>> ExtractAndDecryptFramedPayloadFromCbor(
    const std::vector<uint8_t>& cbor_data,
    const EVP_HPKE_KEY& hpke_private_key) {
  std::optional<std::vector<uint8_t>> payload =
      ExtractPayloadFromCbor(cbor_data);
  std::optional<std::vector<uint8_t>> shared_info =
      CreateSharedInfoForEncryption(cbor_data);

  if (payload.has_value() && shared_info.has_value()) {
    return DecryptPayloadWithHpke(std::move(payload.value()), hpke_private_key,
                                  std::move(shared_info.value()));
  }
  return std::nullopt;
}

}  // namespace content::PrivateModelTrainingTestUtils
