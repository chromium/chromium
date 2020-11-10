// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/signed_redemption_record_serialization.h"

#include <string>

#include "base/containers/span.h"
#include "base/strings/string_piece.h"
#include "base/traits_bag.h"
#include "net/http/structured_headers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

enum class WithBody { kValid, kInvalid, kAbsent };
enum class WithSignature { kValid, kInvalid, kAbsent };
enum class WithAdditionalMember { kYes, kNo };

// Returns a minimal redemption record-like Structured Headers dictionary to
// help with error handling in the RR deserialization code.
//
// |with_body| specifies whether to include a valid "body" field, a type-unsafe
// one, or none at all; |with_signature| does the same for the "signature"
// field.
//
// If |with_additional_member| is kYes, the dictionary will contain an
// additional member beyond whichever of the body and signature are included.
std::string CreateSerializedDictionary(
    WithBody with_body,
    WithSignature with_signature,
    WithAdditionalMember with_additional_member) {
  net::structured_headers::Dictionary dict;

  switch (with_signature) {
    case WithSignature::kValid:
      dict["signature"] = net::structured_headers::ParameterizedMember(
          net::structured_headers::Item(
              "example signature",
              net::structured_headers::Item::kByteSequenceType),
          {});
      break;
    case WithSignature::kInvalid:
      // This isn't a byte sequence, so it's not a valid value corresponding to
      // the "signature" key.
      dict["signature"] = net::structured_headers::ParameterizedMember(
          net::structured_headers::Item(int64_t{5}), {});
      break;
    case WithSignature::kAbsent:
      break;
  }

  switch (with_body) {
    case WithBody::kValid:
      dict["body"] = net::structured_headers::ParameterizedMember(
          net::structured_headers::Item(
              "example body", net::structured_headers::Item::kByteSequenceType),
          {});
      break;
    case WithBody::kInvalid:
      // This isn't a byte sequence, so it's not a valid value corresponding to
      // the "body" key.
      dict["body"] = net::structured_headers::ParameterizedMember(
          net::structured_headers::Item(int64_t{5}), {});
      break;
    case WithBody::kAbsent:
      break;
  }

  if (with_additional_member == WithAdditionalMember::kYes) {
    dict["additional"] = net::structured_headers::ParameterizedMember(
        net::structured_headers::Item(int64_t{5}), {});
  }

  return *net::structured_headers::SerializeDictionary(dict);
}

}  // namespace

TEST(RedemptionRecordSerialization, SerializeAndParse) {
  std::string body = "body";
  std::string signature = "example signature";
  base::Optional<std::string> maybe_serialized =
      ConstructRedemptionRecord(base::as_bytes(base::make_span(body)),
                                base::as_bytes(base::make_span(signature)));
  ASSERT_TRUE(maybe_serialized);

  std::string obtained_body;
  std::string obtained_signature;
  EXPECT_TRUE(ParseTrustTokenRedemptionRecord(*maybe_serialized, &obtained_body,
                                              &obtained_signature));
  EXPECT_EQ(obtained_body, body);
  EXPECT_EQ(obtained_signature, signature);
}

TEST(RedemptionRecordSerialization, SerializeAndParseNullptrParams) {
  // Make sure ParseTrustTokenRedemptionRecord doesn't blow up (i.e.,
  // dereference a null pointer) when its optional params aren't provided.
  std::string body = "example body";
  std::string signature = "example signature";
  base::Optional<std::string> maybe_serialized =
      ConstructRedemptionRecord(base::as_bytes(base::make_span(body)),
                                base::as_bytes(base::make_span(signature)));
  ASSERT_TRUE(maybe_serialized);

  EXPECT_TRUE(
      ParseTrustTokenRedemptionRecord(*maybe_serialized, nullptr, nullptr));
}

TEST(RedemptionRecordSerialization, ParseNotDictionary) {
  // Parse should reject objects that aren't Structured Headers dictionaries.
  EXPECT_FALSE(ParseTrustTokenRedemptionRecord(
      "Not a Structured Headers dictionary", nullptr, nullptr));
}

TEST(RedemptionRecordSerialization, ParseTooSmallDictionary) {
  // Parse should reject Structured Headers dictionaries that aren't size 2.
  EXPECT_FALSE(ParseTrustTokenRedemptionRecord(
      CreateSerializedDictionary(WithBody::kAbsent, WithSignature::kAbsent,
                                 WithAdditionalMember::kNo),
      nullptr, nullptr));
}

TEST(RedemptionRecordSerialization, ParseDictionaryWithTypeUnsafeSignature) {
  // Parse should reject size 2 structured headers dictionaries with members of
  // the wrong type.
  EXPECT_FALSE(ParseTrustTokenRedemptionRecord(
      CreateSerializedDictionary(WithBody::kValid, WithSignature::kInvalid,
                                 WithAdditionalMember::kNo),
      nullptr, nullptr));
}

TEST(RedemptionRecordSerialization, ParseDictionaryWithTypeUnsafeBody) {
  // Parse should reject size 2 structured headers dictionaries with members of
  // the wrong type.
  EXPECT_FALSE(ParseTrustTokenRedemptionRecord(
      CreateSerializedDictionary(WithBody::kInvalid, WithSignature::kValid,
                                 WithAdditionalMember::kNo),
      nullptr, nullptr));
}

TEST(RedemptionRecordSerialization, ParseDictionaryWithExtraMembers) {
  // Parse should reject size >2 structured headers dictionaries.
  EXPECT_FALSE(ParseTrustTokenRedemptionRecord(
      CreateSerializedDictionary(WithBody::kValid, WithSignature::kValid,
                                 WithAdditionalMember::kYes),
      nullptr, nullptr));
}

}  // namespace network
