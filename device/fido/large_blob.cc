// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/large_blob.h"

#include <algorithm>
#include <ostream>

#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "base/ranges/algorithm.h"
#include "components/cbor/reader.h"
#include "components/cbor/writer.h"
#include "crypto/aead.h"
#include "crypto/random.h"
#include "crypto/sha2.h"
#include "device/fido/fido_parsing_utils.h"
#include "device/fido/pin.h"

namespace device {

namespace {
// The number of bytes the large blob validation hash is truncated to.
constexpr size_t kTruncatedHashBytes = 16;
constexpr std::array<uint8_t, 4> kLargeBlobADPrefix = {'b', 'l', 'o', 'b'};
constexpr size_t kAssociatedDataLength = kLargeBlobADPrefix.size() + 8;

std::array<uint8_t, kAssociatedDataLength> GenerateLargeBlobAdditionalData(
    size_t size) {
  std::array<uint8_t, kAssociatedDataLength> additional_data;
  const std::array<uint8_t, 8>& size_array =
      fido_parsing_utils::Uint64LittleEndian(size);
  base::ranges::copy(kLargeBlobADPrefix, additional_data.begin());
  base::ranges::copy(size_array,
                     additional_data.begin() + kLargeBlobADPrefix.size());
  return additional_data;
}

}  // namespace

LargeBlob::LargeBlob(std::vector<uint8_t> compressed_data,
                     uint64_t original_size)
    : compressed_data(std::move(compressed_data)),
      original_size(original_size) {}
LargeBlob::~LargeBlob() = default;
LargeBlob::LargeBlob(const LargeBlob&) = default;
LargeBlob& LargeBlob::operator=(const LargeBlob&) = default;
LargeBlob::LargeBlob(LargeBlob&&) = default;
LargeBlob& LargeBlob::operator=(LargeBlob&&) = default;
bool LargeBlob::operator==(const LargeBlob& other) const {
  return other.compressed_data == compressed_data &&
         other.original_size == original_size;
}

LargeBlobArrayFragment::LargeBlobArrayFragment(std::vector<uint8_t> bytes,
                                               size_t offset)
    : bytes(std::move(bytes)), offset(offset) {}

LargeBlobArrayFragment::~LargeBlobArrayFragment() = default;
LargeBlobArrayFragment::LargeBlobArrayFragment(LargeBlobArrayFragment&&) =
    default;

bool VerifyLargeBlobArrayIntegrity(base::span<const uint8_t> large_blob_array) {
  if (large_blob_array.size() <= kTruncatedHashBytes) {
    return false;
  }
  const size_t trail_offset = large_blob_array.size() - kTruncatedHashBytes;
  std::array<uint8_t, crypto::kSHA256Length> large_blob_hash =
      crypto::SHA256Hash(large_blob_array.first(trail_offset));

  base::span<const uint8_t> large_blob_trail =
      large_blob_array.subspan(trail_offset);
  return std::equal(large_blob_hash.begin(),
                    large_blob_hash.begin() + kTruncatedHashBytes,
                    large_blob_trail.begin(), large_blob_trail.end());
}

// static
LargeBlobsRequest LargeBlobsRequest::ForRead(size_t bytes, size_t offset) {
  DCHECK_GT(bytes, 0u);
  LargeBlobsRequest request;
  request.get_ = bytes;
  request.offset_ = offset;
  return request;
}

// static
LargeBlobsRequest LargeBlobsRequest::ForWrite(LargeBlobArrayFragment fragment,
                                              size_t length) {
  LargeBlobsRequest request;
  if (fragment.offset == 0) {
    request.length_ = length;
  }
  request.offset_ = fragment.offset;
  request.set_ = std::move(fragment.bytes);
  return request;
}

LargeBlobsRequest::LargeBlobsRequest() = default;
LargeBlobsRequest::LargeBlobsRequest(LargeBlobsRequest&& other) = default;
LargeBlobsRequest::~LargeBlobsRequest() = default;

void LargeBlobsRequest::SetPinParam(
    const pin::TokenResponse& pin_uv_auth_token) {
  DCHECK(set_) << "SetPinParam should only be used for write requests";
  std::vector<uint8_t> pin_auth(pin::kPinUvAuthTokenSafetyPadding.begin(),
                                pin::kPinUvAuthTokenSafetyPadding.end());
  pin_auth.insert(pin_auth.end(), kLargeBlobPinPrefix.begin(),
                  kLargeBlobPinPrefix.end());
  const std::array<uint8_t, 4> offset_array =
      fido_parsing_utils::Uint32LittleEndian(offset_);
  pin_auth.insert(pin_auth.end(), offset_array.begin(), offset_array.end());
  std::array<uint8_t, crypto::kSHA256Length> set_hash =
      crypto::SHA256Hash(*set_);
  pin_auth.insert(pin_auth.end(), set_hash.begin(), set_hash.end());
  std::tie(pin_uv_auth_protocol_, pin_uv_auth_param_) =
      pin_uv_auth_token.PinAuth(pin_auth);
}

// static
std::optional<LargeBlobsResponse> LargeBlobsResponse::ParseForRead(
    const size_t bytes_to_read,
    const std::optional<cbor::Value>& cbor_response) {
  if (!cbor_response || !cbor_response->is_map()) {
    return std::nullopt;
  }

  const cbor::Value::MapValue& map = cbor_response->GetMap();
  auto it =
      map.find(cbor::Value(static_cast<int>(LargeBlobsResponseKey::kConfig)));
  if (it == map.end() || !it->second.is_bytestring()) {
    return std::nullopt;
  }

  const std::vector<uint8_t>& config = it->second.GetBytestring();
  if (config.size() > bytes_to_read) {
    return std::nullopt;
  }

  return LargeBlobsResponse(std::move(config));
}

// static
std::optional<LargeBlobsResponse> LargeBlobsResponse::ParseForWrite(
    const std::optional<cbor::Value>& cbor_response) {
  // For writing, we expect an empty response.
  if (cbor_response) {
    return std::nullopt;
  }

  return LargeBlobsResponse();
}

LargeBlobsResponse::LargeBlobsResponse(
    std::optional<std::vector<uint8_t>> config)
    : config_(std::move(config)) {}
LargeBlobsResponse::LargeBlobsResponse(LargeBlobsResponse&& other) = default;
LargeBlobsResponse& LargeBlobsResponse::operator=(LargeBlobsResponse&& other) =
    default;
LargeBlobsResponse::~LargeBlobsResponse() = default;

std::pair<CtapRequestCommand, std::optional<cbor::Value>>
AsCTAPRequestValuePair(const LargeBlobsRequest& request) {
  cbor::Value::MapValue map;
  if (request.get_) {
    map.emplace(static_cast<int>(LargeBlobsRequestKey::kGet), *request.get_);
  }
  if (request.set_) {
    map.emplace(static_cast<int>(LargeBlobsRequestKey::kSet), *request.set_);
  }
  map.emplace(static_cast<int>(LargeBlobsRequestKey::kOffset), request.offset_);
  if (request.length_) {
    map.emplace(static_cast<int>(LargeBlobsRequestKey::kLength),
                *request.length_);
  }
  if (request.pin_uv_auth_param_) {
    map.emplace(static_cast<int>(LargeBlobsRequestKey::kPinUvAuthParam),
                *request.pin_uv_auth_param_);
  }
  if (request.pin_uv_auth_protocol_) {
    map.emplace(static_cast<int>(LargeBlobsRequestKey::kPinUvAuthProtocol),
                static_cast<uint8_t>(*request.pin_uv_auth_protocol_));
  }
  return std::make_pair(CtapRequestCommand::kAuthenticatorLargeBlobs,
                        cbor::Value(std::move(map)));
}

// static.
std::optional<LargeBlobData> LargeBlobData::Parse(const cbor::Value& value) {
  if (!value.is_map()) {
    return std::nullopt;
  }
  const cbor::Value::MapValue& map = value.GetMap();
  auto ciphertext_it =
      map.find(cbor::Value(static_cast<int>(LargeBlobDataKeys::kCiphertext)));
  if (ciphertext_it == map.end() || !ciphertext_it->second.is_bytestring()) {
    return std::nullopt;
  }
  const auto* nonce = base::FindOrNull(
      map, cbor::Value(static_cast<int>(LargeBlobDataKeys::kNonce)));
  if (!nonce || !nonce->is_bytestring()) {
    return std::nullopt;
  }
  auto sized_nonce_span = base::span(nonce->GetBytestring())
                              .to_fixed_extent<kLargeBlobArrayNonceLength>();
  if (!sized_nonce_span) {
    return std::nullopt;
  }
  auto orig_size_it =
      map.find(cbor::Value(static_cast<int>(LargeBlobDataKeys::kOrigSize)));
  if (orig_size_it == map.end() || !orig_size_it->second.is_unsigned()) {
    return std::nullopt;
  }
  return LargeBlobData(ciphertext_it->second.GetBytestring(), *sized_nonce_span,
                       orig_size_it->second.GetUnsigned());
}

LargeBlobData::LargeBlobData(
    std::vector<uint8_t> ciphertext,
    base::span<const uint8_t, kLargeBlobArrayNonceLength> nonce,
    int64_t orig_size)
    : ciphertext_(std::move(ciphertext)), orig_size_(std::move(orig_size)) {
  base::ranges::copy(nonce, nonce_.begin());
}
LargeBlobData::LargeBlobData(LargeBlobKey key, LargeBlob large_blob)
    : orig_size_(large_blob.original_size) {
  crypto::Aead aead(crypto::Aead::AeadAlgorithm::AES_256_GCM);
  aead.Init(key);
  crypto::RandBytes(nonce_);
  ciphertext_ = aead.Seal(large_blob.compressed_data, nonce_,
                          GenerateLargeBlobAdditionalData(orig_size_));
}
LargeBlobData::LargeBlobData(LargeBlobData&&) = default;
LargeBlobData& LargeBlobData::operator=(LargeBlobData&&) = default;
LargeBlobData::~LargeBlobData() = default;

bool LargeBlobData::operator==(const LargeBlobData& other) const {
  return ciphertext_ == other.ciphertext_ && nonce_ == other.nonce_ &&
         orig_size_ == other.orig_size_;
}

std::optional<LargeBlob> LargeBlobData::Decrypt(LargeBlobKey key) const {
  crypto::Aead aead(crypto::Aead::AeadAlgorithm::AES_256_GCM);
  aead.Init(key);
  std::optional<std::vector<uint8_t>> compressed_data = aead.Open(
      ciphertext_, nonce_, GenerateLargeBlobAdditionalData(orig_size_));
  if (!compressed_data) {
    return std::nullopt;
  }
  return LargeBlob(*compressed_data, orig_size_);
}

cbor::Value LargeBlobData::AsCBOR() const {
  cbor::Value::MapValue map;
  map.emplace(static_cast<int>(LargeBlobDataKeys::kCiphertext), ciphertext_);
  map.emplace(static_cast<int>(LargeBlobDataKeys::kNonce), nonce_);
  map.emplace(static_cast<int>(LargeBlobDataKeys::kOrigSize), orig_size_);
  return cbor::Value(map);
}

LargeBlobArrayReader::LargeBlobArrayReader() = default;
LargeBlobArrayReader::LargeBlobArrayReader(LargeBlobArrayReader&&) = default;
LargeBlobArrayReader::~LargeBlobArrayReader() = default;

void LargeBlobArrayReader::Append(const std::vector<uint8_t>& fragment) {
  bytes_.insert(bytes_.end(), fragment.begin(), fragment.end());
}

std::optional<cbor::Value::ArrayValue> LargeBlobArrayReader::Materialize() {
  if (!VerifyLargeBlobArrayIntegrity(bytes_)) {
    return std::nullopt;
  }

  base::span<const uint8_t> cbor_bytes =
      base::span(bytes_).first(bytes_.size() - kTruncatedHashBytes);
  std::optional<cbor::Value> cbor = cbor::Reader::Read(cbor_bytes);
  if (!cbor || !cbor->is_array()) {
    return std::nullopt;
  }

  cbor::Value::ArrayValue large_blob_array;
  const cbor::Value::ArrayValue& array = cbor->GetArray();
  for (const cbor::Value& value : array) {
    large_blob_array.push_back(value.Clone());
  }

  return large_blob_array;
}

LargeBlobArrayWriter::LargeBlobArrayWriter(
    cbor::Value::ArrayValue large_blob_array) {
  bytes_ = *cbor::Writer::Write(cbor::Value(std::move(large_blob_array)));
  std::array<uint8_t, crypto::kSHA256Length> large_blob_hash =
      crypto::SHA256Hash(bytes_);
  bytes_.insert(bytes_.end(), large_blob_hash.begin(),
                large_blob_hash.begin() + kTruncatedHashBytes);
  DCHECK(VerifyLargeBlobArrayIntegrity(bytes_));
}
LargeBlobArrayWriter::LargeBlobArrayWriter(LargeBlobArrayWriter&&) = default;
LargeBlobArrayWriter::~LargeBlobArrayWriter() = default;

LargeBlobArrayFragment LargeBlobArrayWriter::Pop(size_t length) {
  CHECK(has_remaining_fragments());
  length = std::min(length, bytes_.size() - offset_);

  LargeBlobArrayFragment fragment{
      fido_parsing_utils::Materialize(
          base::span(bytes_).subspan(offset_, length)),
      offset_};
  offset_ += length;
  return fragment;
}

}  // namespace device
