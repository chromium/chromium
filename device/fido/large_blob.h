// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_LARGE_BLOB_H_
#define DEVICE_FIDO_LARGE_BLOB_H_

#include <cstdint>
#include <cstdlib>
#include <vector>

#include "base/component_export.h"
#include "device/fido/fido_constants.h"
#include "device/fido/pin.h"

namespace device {

// https://drafts.fidoalliance.org/fido-2/stable-links-to-latest/fido-client-to-authenticator-protocol.html#largeBlobsRW
enum class LargeBlobsRequestKey : uint8_t {
  kGet = 0x01,
  kSet = 0x02,
  kOffset = 0x03,
  kLength = 0x04,
  kPinUvAuthParam = 0x05,
  kPinUvAuthProtocol = 0x06,
};

// https://drafts.fidoalliance.org/fido-2/stable-links-to-latest/fido-client-to-authenticator-protocol.html#largeBlobsRW
enum class LargeBlobsResponseKey : uint8_t {
  kConfig = 0x01,
};

// https://drafts.fidoalliance.org/fido-2/stable-links-to-latest/fido-client-to-authenticator-protocol.html#large-blob
enum class LargeBlobDataKeys : uint8_t {
  kCiphertext = 0x01,
  kNonce = 0x02,
  kOrigSize = 0x03,
};

using LargeBlobKey = std::array<uint8_t, kLargeBlobKeyLength>;

constexpr size_t kLargeBlobDefaultMaxFragmentLength = 960;
constexpr size_t kLargeBlobReadEncodingOverhead = 64;
constexpr size_t kLargeBlobArrayNonceLength = 12;
constexpr std::array<uint8_t, 2> kLargeBlobPinPrefix = {0x0c, 0x00};

struct COMPONENT_EXPORT(DEVICE_FIDO) LargeBlobArrayFragment {
  LargeBlobArrayFragment(std::vector<uint8_t> bytes, size_t offset);
  ~LargeBlobArrayFragment();
  LargeBlobArrayFragment(const LargeBlobArrayFragment&) = delete;
  LargeBlobArrayFragment operator=(const LargeBlobArrayFragment&) = delete;
  LargeBlobArrayFragment(LargeBlobArrayFragment&&);
  const std::vector<uint8_t> bytes;
  const size_t offset;
};

COMPONENT_EXPORT(DEVICE_FIDO)
bool VerifyLargeBlobArrayIntegrity(base::span<const uint8_t> large_blob_array);

class LargeBlobsRequest {
 public:
  ~LargeBlobsRequest();
  LargeBlobsRequest(const LargeBlobsRequest&) = delete;
  LargeBlobsRequest operator=(const LargeBlobsRequest&) = delete;
  LargeBlobsRequest(LargeBlobsRequest&& other);

  static LargeBlobsRequest ForRead(size_t bytes, size_t offset);
  static LargeBlobsRequest ForWrite(LargeBlobArrayFragment fragment,
                                    size_t length);

  void SetPinParam(const pin::TokenResponse& pin_uv_auth_token);

  friend std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
  AsCTAPRequestValuePair(const LargeBlobsRequest& request);

 private:
  LargeBlobsRequest();

  base::Optional<int64_t> get_;
  base::Optional<std::vector<uint8_t>> set_;
  int64_t offset_ = 0;
  base::Optional<int64_t> length_;
  base::Optional<std::vector<uint8_t>> pin_uv_auth_param_;
  base::Optional<PINUVAuthProtocol> pin_uv_auth_protocol_;
};

class LargeBlobsResponse {
 public:
  LargeBlobsResponse(const LargeBlobsResponse&) = delete;
  LargeBlobsResponse operator=(const LargeBlobsResponse&) = delete;
  LargeBlobsResponse(LargeBlobsResponse&& other);
  LargeBlobsResponse& operator=(LargeBlobsResponse&&);
  ~LargeBlobsResponse();

  static base::Optional<LargeBlobsResponse> ParseForRead(
      size_t bytes_to_read,
      const base::Optional<cbor::Value>& cbor_response);
  static base::Optional<LargeBlobsResponse> ParseForWrite(
      const base::Optional<cbor::Value>& cbor_response);

  base::Optional<std::vector<uint8_t>> config() { return config_; }

 private:
  explicit LargeBlobsResponse(
      base::Optional<std::vector<uint8_t>> config = base::nullopt);

  base::Optional<std::vector<uint8_t>> config_;
};

// Represents the large-blob map structure
// https://drafts.fidoalliance.org/fido-2/stable-links-to-latest/fido-client-to-authenticator-protocol.html#large-blob
class COMPONENT_EXPORT(DEVICE_FIDO) LargeBlobData {
 public:
  static base::Optional<LargeBlobData> Parse(const cbor::Value& cbor_response);

  LargeBlobData(LargeBlobKey key, base::span<const uint8_t> blob);
  LargeBlobData(const LargeBlobData&) = delete;
  LargeBlobData operator=(const LargeBlobData&) = delete;
  LargeBlobData(LargeBlobData&&);
  LargeBlobData& operator=(LargeBlobData&&);
  ~LargeBlobData();
  bool operator==(const LargeBlobData&) const;

  base::Optional<std::vector<uint8_t>> Decrypt(LargeBlobKey key) const;
  cbor::Value::MapValue AsCBOR() const;

 private:
  LargeBlobData(std::vector<uint8_t> ciphertext,
                base::span<const uint8_t, kLargeBlobArrayNonceLength> nonce,
                int64_t orig_size);
  std::vector<uint8_t> ciphertext_;
  std::array<uint8_t, kLargeBlobArrayNonceLength> nonce_;
  int64_t orig_size_;
};

// Reading large blob arrays is done in chunks. This class provides facilities
// to assemble together those chunks.
class COMPONENT_EXPORT(DEVICE_FIDO) LargeBlobArrayReader {
 public:
  LargeBlobArrayReader();
  LargeBlobArrayReader(const LargeBlobArrayReader&) = delete;
  LargeBlobArrayReader operator=(const LargeBlobArrayReader&) = delete;
  LargeBlobArrayReader(LargeBlobArrayReader&&);
  ~LargeBlobArrayReader();

  // Appends a fragment to the large blob array.
  void Append(const std::vector<uint8_t>& fragment);

  // Verifies the integrity of the large blob array. This should be called after
  // all fragments have been |Append|ed.
  // If successful, parses and returns the array.
  base::Optional<std::vector<LargeBlobData>> Materialize();

  // Returns the current size of the array fragments.
  size_t size() const { return bytes_.size(); }

 private:
  std::vector<uint8_t> bytes_;
};

// Writing large blob arrays is done in chunks. This class provides facilities
// to divide a blob into chunks.
class COMPONENT_EXPORT(DEVICE_FIDO) LargeBlobArrayWriter {
 public:
  explicit LargeBlobArrayWriter(
      const std::vector<LargeBlobData>& large_blob_array);
  LargeBlobArrayWriter(const LargeBlobArrayWriter&) = delete;
  LargeBlobArrayWriter operator=(const LargeBlobArrayWriter&) = delete;
  LargeBlobArrayWriter(LargeBlobArrayWriter&&);
  ~LargeBlobArrayWriter();

  // Extracts a fragment with |length|. Can only be called if
  // has_remaining_fragments() is true.
  LargeBlobArrayFragment Pop(size_t length);

  // Returns the current size of the array fragments.
  size_t size() const { return bytes_.size(); }

  // Returns true if there are remaining fragments to be written, false
  // otherwise.
  bool has_remaining_fragments() const { return offset_ < size(); }

  void set_bytes_for_testing(std::vector<uint8_t> bytes) {
    bytes_ = std::move(bytes);
  }

 private:
  std::vector<uint8_t> bytes_;
  size_t offset_ = 0;
};

std::pair<CtapRequestCommand, base::Optional<cbor::Value>>
AsCTAPRequestValuePair(const LargeBlobsRequest& request);

}  // namespace device

#endif  // DEVICE_FIDO_LARGE_BLOB_H_
