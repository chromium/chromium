// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/ec_public_key.h"

#include <utility>

#include "components/cbor/writer.h"
#include "device/fido/fido_parsing_utils.h"

namespace device {

namespace {
// In a U2F registration response, the key is located after the first byte of
// the response (which is a reserved byte). It's in X9.62 format:
// - a constant 0x04 prefix to indicate an uncompressed key
// - the 32-byte x coordinate
// - the 32-byte y coordinate.
constexpr size_t kReservedLength = 1;
constexpr uint8_t kUncompressedKey = 0x04;
constexpr size_t kFieldElementLength = 32;
}  // namespace

// static
std::unique_ptr<ECPublicKey> ECPublicKey::ExtractFromU2fRegistrationResponse(
    std::string algorithm,
    base::span<const uint8_t> u2f_data) {
  return ParseX962Uncompressed(
      std::move(algorithm),
      fido_parsing_utils::ExtractSuffixSpan(u2f_data, kReservedLength));
}

// static
std::unique_ptr<ECPublicKey> ECPublicKey::ParseX962Uncompressed(
    std::string algorithm,
    base::span<const uint8_t> input) {
  if (input.empty() || input[0] != kUncompressedKey)
    return nullptr;

  const std::vector<uint8_t> x =
      fido_parsing_utils::Extract(input, 1, kFieldElementLength);
  if (x.empty())
    return nullptr;

  const std::vector<uint8_t> y = fido_parsing_utils::Extract(
      input, 1 + kFieldElementLength, kFieldElementLength);
  if (y.empty())
    return nullptr;

  return std::make_unique<ECPublicKey>(std::move(algorithm), std::move(x),
                                       std::move(y));
}

ECPublicKey::ECPublicKey(std::string algorithm,
                         std::vector<uint8_t> x,
                         std::vector<uint8_t> y)
    : PublicKey(std::move(algorithm)),
      x_coordinate_(std::move(x)),
      y_coordinate_(std::move(y)) {
  DCHECK_EQ(x_coordinate_.size(), kFieldElementLength);
  DCHECK_EQ(y_coordinate_.size(), kFieldElementLength);
}

ECPublicKey::~ECPublicKey() = default;

std::vector<uint8_t> ECPublicKey::EncodeAsCOSEKey() const {
  cbor::Value::MapValue map;
  map[cbor::Value(1)] = cbor::Value(2);
  map[cbor::Value(3)] = cbor::Value(-7);
  map[cbor::Value(-1)] = cbor::Value(1);
  map[cbor::Value(-2)] = cbor::Value(x_coordinate_);
  map[cbor::Value(-3)] = cbor::Value(y_coordinate_);
  return *cbor::Writer::Write(cbor::Value(std::move(map)));
}

}  // namespace device
