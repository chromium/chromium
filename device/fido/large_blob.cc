// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/large_blob.h"
#include "base/containers/span.h"
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
  std::copy(kLargeBlobADPrefix.begin(), kLargeBlobADPrefix.end(),
            additional_data.begin());
  std::copy(size_array.begin(), size_array.end(),
            additional_data.begin() + kLargeBlobADPrefix.size());
  return additional_data;
}

}  // namespace

LargeBlobArrayFragment::LargeBlobArrayFragment(const std::vector<uint8_t> bytes,
                                               const size_t offset)
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
      crypto::SHA256Hash(large_blob_array.subspan(0, trail_offset));

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
  pin_uv_auth_protocol_ = pin::kProtocolVersion;
  std::vector<uint8_t> pin_auth(pin::kPinUvAuthTokenSafetyPadding.begin(),
                                pin::kPinUvAuthTokenSafetyPadding.end());
  pin_auth.insert(pin_auth.end(), kLargeBlobPinPrefix.begin(),
                  kLargeBlobPinPrefix.end());
  const std::array<uint8_t, 4> offset_array =
      fido_parsing_utils::Uint32LittleEndian(offset_);
  pin_auth.insert(pin_auth.end(), offset_array.begin(), offset_array.end());
  if (set_) {
    pin_auth.insert(pin_auth.end(), set_->begin(), set_->end());
  }
  pin_uv_auth_param_ = pin_uv_auth_token.PinAuth(pin_auth);
}

// static
base::Optional<LargeBlobsResponse> LargeBlobsResponse::ParseForRead(
    const size_t bytes_to_read,
    const base::Optional<cbor::Value>& cbor_response) {
  if (!cbor_response || !cbor_response->is_map()) {
    return base::nullopt;
  }

  const cbor::Value::MapValue& map = cbor_response->GetMap();
  auto it =
      map.find(cbor::Value(static_cast<int>(LargeBlobsResponseKey::kConfig)));
  if (it == map.end() || !it->second.is_bytestring()) {
    return base::nullopt;
  }

  const std::vector<uint8_t>& config = it->second.GetBytestring();
  if (config.size() > bytes_to_read) {
    return base::nullopt;
  }

  return LargeBlobsResponse(std::move(config));
}

// static
base::Optional<LargeBlobsResponse> LargeBlobsResponse::ParseForWrite(
    const base::Optional<cbor::Value>& cbor_response) {
  // For writing, we expect an empty response.
  if (cbor_response) {
    return base::nullopt;
  }

  return LargeBlobsResponse();
}

LargeBlobsResponse::LargeBlobsResponse(
    base::Optional<std::vector<uint8_t>> config)
    : config_(std::move(config)) {}
LargeBlobsResponse::LargeBlobsResponse(LargeBlobsResponse&& other) = default;
LargeBlobsResponse& LargeBlobsResponse::operator=(LargeBlobsResponse&& other) =
    default;
LargeBlobsResponse::~LargeBlobsResponse() = default;

std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
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
                *request.pin_uv_auth_protocol_);
  }
  return std::make_pair(CtapRequestCommand::kAuthenticatorLargeBlobs,
                        cbor::Value(std::move(map)));
}

// static.
base::Optional<LargeBlobData> LargeBlobData::Parse(const cbor::Value& value) {
  if (!value.is_map()) {
    return base::nullopt;
  }
  const cbor::Value::MapValue& map = value.GetMap();
  auto ciphertext_it =
      map.find(cbor::Value(static_cast<int>(LargeBlobDataKeys::kCiphertext)));
  if (ciphertext_it == map.end() || !ciphertext_it->second.is_bytestring()) {
    return base::nullopt;
  }
  auto nonce_it =
      map.find(cbor::Value(static_cast<int>(LargeBlobDataKeys::kNonce)));
  if (nonce_it == map.end() || !nonce_it->second.is_bytestring() ||
      nonce_it->second.GetBytestring().size() != kLargeBlobArrayNonceLength) {
    return base::nullopt;
  }
  auto orig_size_it =
      map.find(cbor::Value(static_cast<int>(LargeBlobDataKeys::kOrigSize)));
  if (orig_size_it == map.end() || !orig_size_it->second.is_unsigned()) {
    return base::nullopt;
  }
  return LargeBlobData(ciphertext_it->second.GetBytestring(),
                       base::make_span<kLargeBlobArrayNonceLength>(
                           nonce_it->second.GetBytestring()),
                       orig_size_it->second.GetUnsigned());
}

LargeBlobData::LargeBlobData(
    std::vector<uint8_t> ciphertext,
    base::span<const uint8_t, kLargeBlobArrayNonceLength> nonce,
    int64_t orig_size)
    : ciphertext_(std::move(ciphertext)), orig_size_(std::move(orig_size)) {
  std::copy(nonce.begin(), nonce.end(), nonce_.begin());
}
LargeBlobData::LargeBlobData(LargeBlobKey key, std::vector<uint8_t> blob) {
  orig_size_ = blob.size();
  crypto::Aead aead(crypto::Aead::AeadAlgorithm::AES_256_GCM);
  aead.Init(key);
  crypto::RandBytes(nonce_);
  ciphertext_ =
      aead.Seal(blob, nonce_, GenerateLargeBlobAdditionalData(orig_size_));
}
LargeBlobData::LargeBlobData(LargeBlobData&&) = default;
LargeBlobData& LargeBlobData::operator=(LargeBlobData&&) = default;
LargeBlobData::~LargeBlobData() = default;

bool LargeBlobData::operator==(const LargeBlobData& other) const {
  return ciphertext_ == other.ciphertext_ && nonce_ == other.nonce_ &&
         orig_size_ == other.orig_size_;
}

base::Optional<std::vector<uint8_t>> LargeBlobData::Decrypt(
    LargeBlobKey key) const {
  crypto::Aead aead(crypto::Aead::AeadAlgorithm::AES_256_GCM);
  aead.Init(key);
  return aead.Open(ciphertext_, nonce_,
                   GenerateLargeBlobAdditionalData(orig_size_));
}

cbor::Value::MapValue LargeBlobData::AsCBOR() const {
  cbor::Value::MapValue map;
  map.emplace(static_cast<int>(LargeBlobDataKeys::kCiphertext), ciphertext_);
  map.emplace(static_cast<int>(LargeBlobDataKeys::kNonce), nonce_);
  map.emplace(static_cast<int>(LargeBlobDataKeys::kOrigSize), orig_size_);
  return map;
}

LargeBlobArrayReader::LargeBlobArrayReader() = default;
LargeBlobArrayReader::LargeBlobArrayReader(LargeBlobArrayReader&&) = default;
LargeBlobArrayReader::~LargeBlobArrayReader() = default;

void LargeBlobArrayReader::Append(const std::vector<uint8_t>& fragment) {
  bytes_.insert(bytes_.end(), fragment.begin(), fragment.end());
}

base::Optional<std::vector<LargeBlobData>> LargeBlobArrayReader::Materialize() {
  if (!VerifyLargeBlobArrayIntegrity(bytes_)) {
    return base::nullopt;
  }

  base::span<const uint8_t> cbor_bytes =
      base::make_span(bytes_.data(), bytes_.size() - kTruncatedHashBytes);
  base::Optional<cbor::Value> cbor = cbor::Reader::Read(cbor_bytes);
  if (!cbor || !cbor->is_array()) {
    return base::nullopt;
  }

  std::vector<LargeBlobData> large_blob_array;
  const cbor::Value::ArrayValue& array = cbor->GetArray();
  for (const cbor::Value& value : array) {
    base::Optional<LargeBlobData> large_blob_data = LargeBlobData::Parse(value);
    if (!large_blob_data) {
      continue;
    }

    large_blob_array.emplace_back(std::move(*large_blob_data));
  }

  return large_blob_array;
}

LargeBlobArrayWriter::LargeBlobArrayWriter(
    const std::vector<LargeBlobData>& large_blob_array) {
  cbor::Value::ArrayValue array;
  for (const LargeBlobData& large_blob_data : large_blob_array) {
    array.emplace_back(large_blob_data.AsCBOR());
  }
  bytes_ = *cbor::Writer::Write(cbor::Value(array));

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
          base::make_span(bytes_.data() + offset_, length)),
      offset_};
  offset_ += length;
  return fragment;
}

}  // namespace device
