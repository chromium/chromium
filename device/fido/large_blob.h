// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_LARGE_BLOB_H_
#define DEVICE_FIDO_LARGE_BLOB_H_

#include <cstdint>
#include <cstdlib>
#include <optional>
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
constexpr size_t kMinLargeBlobSize = 1024;
constexpr std::array<uint8_t, 2> kLargeBlobPinPrefix = {0x0c, 0x00};

// A complete but still compressed large blob.
struct COMPONENT_EXPORT(DEVICE_FIDO) LargeBlob {
  LargeBlob(std::vector<uint8_t> compressed_data, uint64_t original_size);
  ~LargeBlob();
  LargeBlob(const LargeBlob&);
  LargeBlob& operator=(const LargeBlob&);
  LargeBlob(LargeBlob&&);
  LargeBlob& operator=(LargeBlob&&);
  bool operator==(const LargeBlob&) const;

  // The DEFLATEd large blob bytes.
  std::vector<uint8_t> compressed_data;

  // Uncompressed, original size of the large blob.
  uint64_t original_size;
};

struct COMPONENT_EXPORT(DEVICE_FIDO) LargeBlobArrayFragment {
  LargeBlobArrayFragment(std::vector<uint8_t> bytes, size_t offset);
  ~LargeBlobArrayFragment();
  LargeBlobArrayFragment(const LargeBlobArrayFragment&) = delete;
  LargeBlobArrayFragment& operator=(const LargeBlobArrayFragment&) = delete;
  LargeBlobArrayFragment(LargeBlobArrayFragment&&);

  std::vector<uint8_t> bytes;
  size_t offset;
};

COMPONENT_EXPORT(DEVICE_FIDO)
bool VerifyLargeBlobArrayIntegrity(base::span<const uint8_t> large_blob_array);

class LargeBlobsRequest {
 public:
  ~LargeBlobsRequest();
  LargeBlobsRequest(const LargeBlobsRequest&) = delete;
  LargeBlobsRequest& operator=(const LargeBlobsRequest&) = delete;
  LargeBlobsRequest(LargeBlobsRequest&& other);

  static LargeBlobsRequest ForRead(size_t bytes, size_t offset);
  static LargeBlobsRequest ForWrite(LargeBlobArrayFragment fragment,
                                    size_t length);

  void SetPinParam(const pin::TokenResponse& pin_uv_auth_token);

  friend std::pair<CtapRequestCommand, std::optional<cbor::Value>>
  AsCTAPRequestValuePair(const LargeBlobsRequest& request);

 private:
  LargeBlobsRequest();

  std::optional<int64_t> get_;
  std::optional<std::vector<uint8_t>> set_;
  int64_t offset_ = 0;
  std::optional<int64_t> length_;
  std::optional<std::vector<uint8_t>> pin_uv_auth_param_;
  std::optional<PINUVAuthProtocol> pin_uv_auth_protocol_;
};

class LargeBlobsResponse {
 public:
  LargeBlobsResponse(const LargeBlobsResponse&) = delete;
  LargeBlobsResponse& operator=(const LargeBlobsResponse&) = delete;
  LargeBlobsResponse(LargeBlobsResponse&& other);
  LargeBlobsResponse& operator=(LargeBlobsResponse&&);
  ~LargeBlobsResponse();

  static std::optional<LargeBlobsResponse> ParseForRead(
      size_t bytes_to_read,
      const std::optional<cbor::Value>& cbor_response);
  static std::optional<LargeBlobsResponse> ParseForWrite(
      const std::optional<cbor::Value>& cbor_response);

  std::optional<std::vector<uint8_t>> config() { return config_; }

 private:
  explicit LargeBlobsResponse(
      std::optional<std::vector<uint8_t>> config = std::nullopt);

  std::optional<std::vector<uint8_t>> config_;
};

// Represents the large-blob map structure
// https://drafts.fidoalliance.org/fido-2/stable-links-to-latest/fido-client-to-authenticator-protocol.html#large-blob
class COMPONENT_EXPORT(DEVICE_FIDO) LargeBlobData {
 public:
  static std::optional<LargeBlobData> Parse(const cbor::Value& cbor_response);

  LargeBlobData(LargeBlobKey key, LargeBlob large_blob);
  LargeBlobData(const LargeBlobData&) = delete;
  LargeBlobData& operator=(const LargeBlobData&) = delete;
  LargeBlobData(LargeBlobData&&);
  LargeBlobData& operator=(LargeBlobData&&);
  ~LargeBlobData();
  bool operator==(const LargeBlobData&) const;

  std::optional<LargeBlob> Decrypt(LargeBlobKey key) const;
  cbor::Value AsCBOR() const;

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
  LargeBlobArrayReader& operator=(const LargeBlobArrayReader&) = delete;
  LargeBlobArrayReader(LargeBlobArrayReader&&);
  ~LargeBlobArrayReader();

  // Appends a fragment to the large blob array.
  void Append(const std::vector<uint8_t>& fragment);

  // Verifies the integrity of the large blob array. This should be called after
  // all fragments have been |Append|ed.
  // If successful, parses and returns the array.
  std::optional<cbor::Value::ArrayValue> Materialize();

  // Returns the current size of the array fragments.
  size_t size() const { return bytes_.size(); }

 private:
  std::vector<uint8_t> bytes_;
};

// Writing large blob arrays is done in chunks. This class provides facilities
// to divide a blob into chunks.
class COMPONENT_EXPORT(DEVICE_FIDO) LargeBlobArrayWriter {
 public:
  explicit LargeBlobArrayWriter(cbor::Value::ArrayValue large_blob_array);
  LargeBlobArrayWriter(const LargeBlobArrayWriter&) = delete;
  LargeBlobArrayWriter& operator=(const LargeBlobArrayWriter&) = delete;
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

std::pair<CtapRequestCommand, std::optional<cbor::Value>>
AsCTAPRequestValuePair(const LargeBlobsRequest& request);

}  // namespace device

#endif  // DEVICE_FIDO_LARGE_BLOB_H_
