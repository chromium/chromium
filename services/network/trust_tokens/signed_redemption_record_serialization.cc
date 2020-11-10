// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/signed_redemption_record_serialization.h"
#include "components/cbor/reader.h"
#include "components/cbor/values.h"
#include "net/http/structured_headers.h"

namespace network {

namespace {

const char kRedemptionRecordBodyKey[] = "body";
const char kRedemptionRecordSignatureKey[] = "signature";

}  // namespace

base::Optional<std::string> ConstructRedemptionRecord(
    base::span<const uint8_t> body,
    base::span<const uint8_t> signature) {
  net::structured_headers::Dictionary dictionary;

  // Stunningly, this is the easiest way to add a byte array-typed value to a
  // net::structured_headers::Dictionary.
  auto make_value_for_dict = [](base::span<const uint8_t> value) {
    return net::structured_headers::ParameterizedMember(
        net::structured_headers::Item(
            std::string(reinterpret_cast<const char*>(value.data()),
                        value.size()),
            net::structured_headers::Item::kByteSequenceType),
        net::structured_headers::Parameters{});
  };

  dictionary[kRedemptionRecordBodyKey] = make_value_for_dict(body);
  dictionary[kRedemptionRecordSignatureKey] = make_value_for_dict(signature);

  return net::structured_headers::SerializeDictionary(dictionary);
}

bool ParseTrustTokenRedemptionRecord(base::StringPiece record,
                                     std::string* body_out,
                                     std::string* signature_out) {
  base::Optional<net::structured_headers::Dictionary> maybe_dictionary =
      net::structured_headers::ParseDictionary(record);
  if (!maybe_dictionary)
    return false;

  if (maybe_dictionary->size() != 2u)
    return false;

  if (!maybe_dictionary->contains("body") ||
      !maybe_dictionary->contains("signature")) {
    return false;
  }

  net::structured_headers::Item& body_item =
      maybe_dictionary->at("body").member.front().item;
  if (!body_item.is_byte_sequence())
    return false;
  if (body_out)
    *body_out = body_item.GetString();  // GetString gets a byte sequence, too.

  net::structured_headers::Item& signature_item =
      maybe_dictionary->at("signature").member.front().item;
  if (!signature_item.is_byte_sequence())
    return false;
  if (signature_out)
    *signature_out = signature_item.GetString();

  return true;
}

}  // namespace network
