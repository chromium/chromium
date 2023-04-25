// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/trust_tokens/trust_token_key_commitment_parser.h"

#include "base/base64.h"
#include "base/json/json_reader.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/struct_ptr.h"
#include "services/network/public/mojom/trust_tokens.mojom.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;
using ::testing::Truly;
using ::testing::UnorderedElementsAre;

namespace network {

namespace {

// For convenience, define a matcher for checking mojo struct pointer equality
// in order to simplify validating results returning containers of points.
template <typename T>
decltype(auto) EqualsMojo(const mojo::StructPtr<T>& value) {
  return Truly([&](const mojo::StructPtr<T>& candidate) {
    return mojo::Equals(candidate, value);
  });
}

}  // namespace

TEST(TrustTokenKeyCommitmentParser, RejectsEmpty) {
  // If the input isn't valid JSON, we should
  // reject it. In particular, we should reject
  // empty input.

  EXPECT_FALSE(TrustTokenKeyCommitmentParser().Parse(""));
}

TEST(TrustTokenKeyCommitmentParser, RejectsNonemptyMalformed) {
  // If the input isn't valid JSON, we should
  // reject it.
  const char input[] = "certainly not valid JSON";

  // Sanity check that the input is not valid JSON.
  ASSERT_FALSE(base::JSONReader::Read(input));

  EXPECT_FALSE(TrustTokenKeyCommitmentParser().Parse(input));
}

TEST(TrustTokenKeyCommitmentParser, RejectsNonDictionaryInput) {
  // The outermost value should be a dictionary.

  // Valid JSON, but not a dictionary.
  const char input[] = "5";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_FALSE(TrustTokenKeyCommitmentParser().Parse(input));
}

TEST(TrustTokenKeyCommitmentParser, AcceptsMinimal) {
  std::string input =
      R"( { "PrivateStateTokenV1PMB": {
                "protocol_version": "PrivateStateTokenV1PMB",
                "id": 1,
                "batchsize": 5
        }} )";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  auto expectation = mojom::TrustTokenKeyCommitmentResult::New();
  expectation->protocol_version =
      mojom::TrustTokenProtocolVersion::kPrivateStateTokenV1Pmb;
  expectation->id = 1;
  expectation->batch_size = 5;

  EXPECT_THAT(TrustTokenKeyCommitmentParser().Parse(input),
              EqualsMojo(expectation));
}

TEST(TrustTokenKeyCommitmentParser, RejectsKeyWithTypeUnsafeValue) {
  const std::string input = R"({ "PrivateStateTokenV1PMB": {
            "protocol_version": "PrivateStateTokenV1PMB",
            "id": 1,
            "batchsize": 5,
            "keys": 42
         }})";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  // Keys should be a dictionary, so this result shouldn't parse.
  EXPECT_FALSE(TrustTokenKeyCommitmentParser().Parse(input));
}

TEST(TrustTokenKeyCommitmentParser, RejectsKeyWithTypeUnsafeKeyLabel) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  base::Time one_minute_from_now = base::Time::Now() + base::Minutes(1);
  int64_t one_minute_from_now_in_micros =
      (one_minute_from_now - base::Time::UnixEpoch()).InMicroseconds();

  // (The expiry will likely exceed the JSON spec's maximum integer value, so
  // it's encoded as a string.)
  const std::string input = base::StringPrintf(
      R"({ "PrivateStateTokenV1PMB": {
            "protocol_version": "PrivateStateTokenV1PMB",
            "id": 1,
            "batchsize": 5,
            "keys": {
              "this label is not an integer": {
                "Y": "akey",
                "expiry": "%s"
              }
            }
         }})",
      base::NumberToString(one_minute_from_now_in_micros).c_str());

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  // Key labels must be integers in the representable range of uint32_t, so this
  // result shouldn't parse.
  EXPECT_FALSE(TrustTokenKeyCommitmentParser().Parse(input));
}

TEST(TrustTokenKeyCommitmentParser, RejectsKeyWithKeyLabelTooSmall) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  base::Time one_minute_from_now = base::Time::Now() + base::Minutes(1);
  int64_t one_minute_from_now_in_micros =
      (one_minute_from_now - base::Time::UnixEpoch()).InMicroseconds();

  const std::string input = base::StringPrintf(
      R"({ "PrivateStateTokenV1PMB": {
            "protocol_version": "PrivateStateTokenV1PMB",
            "id": 1,
            "batchsize": 5,
            "keys": {
              "-1": {
                "Y": "akey",
                "expiry": "%s"
              }
            }
         }})",
      base::NumberToString(one_minute_from_now_in_micros).c_str());

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  // Key labels must be integers in the representable range of uint32_t, so this
  // result shouldn't parse.
  EXPECT_FALSE(TrustTokenKeyCommitmentParser().Parse(input));
}

TEST(TrustTokenKeyCommitmentParser, RejectsKeyWithKeyLabelTooLarge) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  base::Time one_minute_from_now = base::Time::Now() + base::Minutes(1);
  int64_t one_minute_from_now_in_micros =
      (one_minute_from_now - base::Time::UnixEpoch()).InMicroseconds();

  const std::string input = base::StringPrintf(
      R"({ "PrivateStateTokenV1PMB": {
            "protocol_version": "PrivateStateTokenV1PMB", "id": 1,
            "batchsize": 5,
            "keys": {
              "1000000000000": {
                "Y": "akey",
                "expiry": "%s"
              }
            }
         }})",
      base::NumberToString(one_minute_from_now_in_micros).c_str());

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  // Key labels must be integers in the representable range of uint32_t, so this
  // result shouldn't parse.
  EXPECT_FALSE(TrustTokenKeyCommitmentParser().Parse(input));
}

TEST(TrustTokenKeyCommitmentParser, RejectsOtherwiseValidButNonBase64Key) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  base::Time one_minute_from_now = base::Time::Now() + base::Minutes(1);
  int64_t one_minute_from_now_in_micros =
      (one_minute_from_now - base::Time::UnixEpoch()).InMicroseconds();

  const std::string input = base::StringPrintf(
      R"({ "PrivateStateTokenV1PMB": {
            "protocol_version": "PrivateStateTokenV1PMB", "id": 1,
            "batchsize": 5,
            "keys": {
              "1": {
                "Y": "this key isn't valid base64, so it should be rejected",
                "expiry": "%s"
              }
            }
         }})",
      base::NumberToString(one_minute_from_now_in_micros).c_str());

  // Sanity check that the input is actually valid JSON,
  // and that the given time is valid.
  ASSERT_TRUE(base::JSONReader::Read(input));

  EXPECT_FALSE(TrustTokenKeyCommitmentParser().Parse(input));
}

TEST(TrustTokenKeyCommitmentParser, AcceptsKeyWithExpiryAndBody) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  base::Time one_minute_from_now = base::Time::Now() + base::Minutes(1);
  int64_t one_minute_from_now_in_micros =
      (one_minute_from_now - base::Time::UnixEpoch()).InMicroseconds();

  const std::string input = base::StringPrintf(
      R"({ "PrivateStateTokenV1PMB": {
            "protocol_version": "PrivateStateTokenV1PMB", "id": 1,
            "batchsize": 5,
            "keys": {"1": { "Y": "akey", "expiry": "%s" }}
         }})",
      base::NumberToString(one_minute_from_now_in_micros).c_str());

  // Sanity check that the input is actually valid JSON,
  // and that the given time is valid.
  ASSERT_TRUE(base::JSONReader::Read(input));

  auto my_key = mojom::TrustTokenVerificationKey::New();
  ASSERT_TRUE(base::Base64Decode("akey", &my_key->body));
  my_key->expiry = one_minute_from_now;

  auto result = TrustTokenKeyCommitmentParser().Parse(input);
  ASSERT_TRUE(result);
  EXPECT_THAT(result->keys, ElementsAre(EqualsMojo(my_key)));
}

TEST(TrustTokenKeyCommitmentParser, AcceptsMultipleKeys) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  base::Time one_minute_from_now = base::Time::Now() + base::Minutes(1);
  int64_t one_minute_from_now_in_micros =
      (one_minute_from_now - base::Time::UnixEpoch()).InMicroseconds();

  base::Time two_minutes_from_now = base::Time::Now() + base::Minutes(2);
  int64_t two_minutes_from_now_in_micros =
      (two_minutes_from_now - base::Time::UnixEpoch()).InMicroseconds();

  const std::string input = base::StringPrintf(
      R"({ "PrivateStateTokenV1PMB": {
            "protocol_version": "PrivateStateTokenV1PMB", "id": 1,
            "batchsize": 5,
            "keys": {
              "1": { "Y": "akey", "expiry": "%s" },
              "2": { "Y": "aaaa", "expiry": "%s" }}
         }})",
      base::NumberToString(one_minute_from_now_in_micros).c_str(),
      base::NumberToString(two_minutes_from_now_in_micros).c_str());

  // Sanity check that the input is actually valid JSON,
  // and that the given time is valid.
  ASSERT_TRUE(base::JSONReader::Read(input));

  auto a_key = mojom::TrustTokenVerificationKey::New();
  ASSERT_TRUE(base::Base64Decode("akey", &a_key->body));
  a_key->expiry = one_minute_from_now;

  auto another_key = mojom::TrustTokenVerificationKey::New();
  ASSERT_TRUE(base::Base64Decode("aaaa", &another_key->body));
  another_key->expiry = two_minutes_from_now;

  auto result = TrustTokenKeyCommitmentParser().Parse(input);
  ASSERT_TRUE(result);
  EXPECT_THAT(result->keys,
              UnorderedElementsAre(EqualsMojo(a_key), EqualsMojo(another_key)));
}

TEST(TrustTokenKeyCommitmentParser, RejectsKeyWithNoExpiry) {
  // If a key has a missing "expiry" field, we should reject the entire
  // record.
  const std::string input =
      R"( {"PrivateStateTokenV1PMB": {
          "protocol_version": "PrivateStateTokenV1PMB", "id": 1,
          "batchsize": 5, "keys": {"1": { "Y": "akey" }} }})";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  // Since the key doesn't have an expiry, reject it.
  EXPECT_FALSE(TrustTokenKeyCommitmentParser().Parse(input));
}

TEST(TrustTokenKeyCommitmentParser, RejectsKeyWithMalformedExpiry) {
  // If a key has a malformed "expiry" field, we should reject the entire
  // record.
  const std::string input =
      R"(
   { "PrivateStateTokenV1PMB": {
     "protocol_version": "PrivateStateTokenV1PMB", "id": 1, "batchsize": 5,
     "keys": {
       "1": {
         "Y": "akey",
         "expiry": "absolutely not a valid timestamp"
       }
     }
   }})";

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  // Since the key doesn't have an expiry, reject it.
  EXPECT_FALSE(TrustTokenKeyCommitmentParser().Parse(input));
}

TEST(TrustTokenKeyCommitmentParser, IgnoreKeyWithExpiryInThePast) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  // Ensure that "one minute ago" yields a nonnegative number of microseconds
  // past the Unix epoch.
  env.AdvanceClock(std::max<base::TimeDelta>(
      base::TimeDelta(),
      base::Time::UnixEpoch() + base::Minutes(1) - base::Time::Now()));

  base::Time one_minute_before_now = base::Time::Now() - base::Minutes(1);
  int64_t one_minute_before_now_in_micros =
      (one_minute_before_now - base::Time::UnixEpoch()).InMicroseconds();

  // If the time has passed a key's "expiry" field, we should reject the entire
  // record.
  const std::string input = base::StringPrintf(
      R"( { "PrivateStateTokenV1PMB": {
            "protocol_version": "PrivateStateTokenV1PMB", "id": 1,
          "batchsize": 5, "keys": {"1": { "Y": "akey", "expiry": "%s" }} }})",
      base::NumberToString(one_minute_before_now_in_micros).c_str());

  // Sanity check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  auto expectation = mojom::TrustTokenKeyCommitmentResult::New();
  expectation->protocol_version =
      mojom::TrustTokenProtocolVersion::kPrivateStateTokenV1Pmb;
  expectation->id = 1;
  expectation->batch_size = 5;

  EXPECT_TRUE(
      mojo::Equals(TrustTokenKeyCommitmentParser().Parse(input), expectation));
}

TEST(TrustTokenKeyCommitmentParser, RejectsKeyWithNoBody) {
  base::test::TaskEnvironment env(
      base::test::TaskEnvironment::TimeSource::MOCK_TIME);

  base::Time one_minute_from_now = base::Time::Now() + base::Minutes(1);
  int64_t one_minute_from_now_in_micros =
      (one_minute_from_now - base::Time::UnixEpoch()).InMicroseconds();

  // If a key has an expiry but is missing its body,
  // we should reject the entire result.
  const std::string input = base::StringPrintf(
      R"( { "PrivateStateTokenV1PMB": {
            "protocol_version": "PrivateStateTokenV1PMB", "id": 1,
            "batchsize": 5, "keys": {"1": { "expiry": "%s" }} }} )",
      base::NumberToString(one_minute_from_now_in_micros).c_str());

  // Sanity check that the input is actually valid JSON,
  // and that the date is valid.
  ASSERT_TRUE(base::JSONReader::Read(input));

  // Since the key doesn't have a body, reject it.
  EXPECT_FALSE(TrustTokenKeyCommitmentParser().Parse(input));
}

TEST(TrustTokenKeyCommitmentParser, RejectsEmptyKey) {
  // If a key has neither an expiry or a body,
  // we should reject the entire result.

  const std::string input =
      R"( { "PrivateStateTokenV1PMB": {
            "protocol_version": "PrivateStateTokenV1PMB", "id": 1,
            "batchsize": 5, "keys": {"1": { }} }})";

  // Sanity check that the input is actually valid JSON,
  // and that the date is valid.
  ASSERT_TRUE(base::JSONReader::Read(input));

  // Since the key doesn't have an expiry or a body, reject it.
  EXPECT_FALSE(TrustTokenKeyCommitmentParser().Parse(input));
}

TEST(TrustTokenKeyCommitmentParser, ParsesBatchSize) {
  std::string input =
      R"({ "PrivateStateTokenV1PMB": {
     "protocol_version": "PrivateStateTokenV1PMB", "id": 1, "batchsize": 5
   }})";
  // Double-check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  mojom::TrustTokenKeyCommitmentResultPtr result =
      TrustTokenKeyCommitmentParser().Parse(input);
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->batch_size);
  EXPECT_EQ(result->batch_size, 5);
}

TEST(TrustTokenKeyCommitmentParser, RejectsMissingBatchSize) {
  std::string input =
      R"({ "PrivateStateTokenV1PMB": {
     "protocol_version": "PrivateStateTokenV1PMB", "id": 1
   }})";
  // Double-check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  mojom::TrustTokenKeyCommitmentResultPtr result =
      TrustTokenKeyCommitmentParser().Parse(input);
  EXPECT_FALSE(result);
}

TEST(TrustTokenKeyCommitmentParser, RejectsNonpositiveBatchSize) {
  std::string input =
      R"({ "PrivateStateTokenV1PMB": {
     "protocol_version": "PrivateStateTokenV1PMB", "id": 1,
     "batchsize": 0
   }})";
  // Double-check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  mojom::TrustTokenKeyCommitmentResultPtr result =
      TrustTokenKeyCommitmentParser().Parse(input);
  EXPECT_FALSE(result);
}

TEST(TrustTokenKeyCommitmentParser, RejectsTypeUnsafeBatchSize) {
  std::string input =
      R"({ "PrivateStateTokenV1PMB": {
     "protocol_version": "PrivateStateTokenV1PMB", "id": 1,
     "batchsize": "not a number"
   }})";
  // Double-check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  mojom::TrustTokenKeyCommitmentResultPtr result =
      TrustTokenKeyCommitmentParser().Parse(input);
  EXPECT_FALSE(result);
}

TEST(TrustTokenKeyCommitmentParser, IgnoresRequestIssuanceLocallyOn) {
  std::string input =
      R"({ "PrivateStateTokenV1PMB": {
     "srrkey": "aaaa",
     "batchsize": 1,
     "protocol_version": "PrivateStateTokenV1PMB",
     "id": 1,
     "request_issuance_locally_on": ["android"],
     "unavailable_local_operation_fallback": "web_issuance"
   }})";
  // Double-check that the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  mojom::TrustTokenKeyCommitmentResultPtr result =
      TrustTokenKeyCommitmentParser().Parse(input);
  ASSERT_TRUE(result);
}

TEST(TrustTokenKeyCommitmentParser, ParsesProtocolVersion) {
  std::string input =
      R"({ "PrivateStateTokenV1PMB": {
     "protocol_version": "PrivateStateTokenV1PMB", "id": 1, "batchsize": 5,
     "srrkey": "aaaa"
   }})";
  // Make sure the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  mojom::TrustTokenKeyCommitmentResultPtr result =
      TrustTokenKeyCommitmentParser().Parse(input);
  ASSERT_TRUE(result);
  EXPECT_EQ(result->protocol_version,
            mojom::TrustTokenProtocolVersion::kPrivateStateTokenV1Pmb);
}

TEST(TrustTokenKeyCommitmentParser, ParsesMultipleProtocolVersion) {
  std::string input =
      R"({ "PrivateStateTokenV1PMB": {
     "protocol_version": "PrivateStateTokenV1PMB", "id": 1, "batchsize": 5,
     "srrkey": "aaaa"
     }, "PrivateStateTokenV1VOPRF": {
     "protocol_version": "PrivateStateTokenV1VOPRF", "id": 1, "batchsize": 5
     }})";
  // Make sure the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  mojom::TrustTokenKeyCommitmentResultPtr result =
      TrustTokenKeyCommitmentParser().Parse(input);
  ASSERT_TRUE(result);
  EXPECT_EQ(result->protocol_version,
            mojom::TrustTokenProtocolVersion::kPrivateStateTokenV1Voprf);
}

TEST(TrustTokenKeyCommitmentParser,
     ParsesMultipleIgnoreUnknownProtocolVersion) {
  std::string input =
      R"({ "PrivateStateTokenV1PMB": {
     "protocol_version": "PrivateStateTokenV1PMB", "id": 1, "batchsize": 5,
     "srrkey": "aaaa"
     }, "PrivateStateTokenJunk": {
     "protocol_version": "PrivateStateTokenJunk", "id": 1, "batchsize": 5
     }})";
  // Make sure the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  mojom::TrustTokenKeyCommitmentResultPtr result =
      TrustTokenKeyCommitmentParser().Parse(input);
  ASSERT_TRUE(result);
  EXPECT_EQ(result->protocol_version,
            mojom::TrustTokenProtocolVersion::kPrivateStateTokenV1Pmb);
}

TEST(TrustTokenKeyCommitmentParser, RejectsBadVersionCommitmentType) {
  std::string input = R"({ "PrivateStateTokenV1PMB": 3})";
  // Make sure the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  mojom::TrustTokenKeyCommitmentResultPtr result =
      TrustTokenKeyCommitmentParser().Parse(input);
  EXPECT_FALSE(result);
}

TEST(TrustTokenKeyCommitmentParser, RejectsMissingProtocolVersion) {
  std::string input =
      R"({ "PrivateStateTokenV1PMB": {
     "id": 1, "batchsize": 5, "srrkey": "aaaa"
   }})";
  // Make sure the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  mojom::TrustTokenKeyCommitmentResultPtr result =
      TrustTokenKeyCommitmentParser().Parse(input);
  EXPECT_FALSE(result);
}

TEST(TrustTokenKeyCommitmentParser, RejectsMismatchedProtocolVersion) {
  std::string input =
      R"({ "PrivateStateTokenV1PMB": {
     "protocol_version": "PrivateStateTokenV1VOPRF", "id": 1, "batchsize": 5
   }})";
  // Make sure the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  mojom::TrustTokenKeyCommitmentResultPtr result =
      TrustTokenKeyCommitmentParser().Parse(input);
  EXPECT_FALSE(result);
}

TEST(TrustTokenKeyCommitmentParser, RejectsUnknownProtocolVersion) {
  std::string input =
      R"({ "PrivateStateTokenJunk": {
     "protocol_version": "PrivateStateTokenJunk", "id": 1, "srrkey": "aaaa",
     "batchsize": 5
   }})";
  // Make sure the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  mojom::TrustTokenKeyCommitmentResultPtr result =
      TrustTokenKeyCommitmentParser().Parse(input);
  EXPECT_FALSE(result);
}

TEST(TrustTokenKeyCommitmentParser, RejectsTypeUnsafeProtocolVersion) {
  std::string input =
      R"({ "PrivateStateTokenV1PMB": {
     "protocol_version": 5, "id": 1, "srrkey": "aaaa",
     "batchsize": 5
   }})";
  // Make sure the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  mojom::TrustTokenKeyCommitmentResultPtr result =
      TrustTokenKeyCommitmentParser().Parse(input);
  EXPECT_FALSE(result);
}

TEST(TrustTokenKeyCommitmentParser, ParsesID) {
  std::string input =
      R"({ "PrivateStateTokenV1PMB": {
     "protocol_version": "PrivateStateTokenV1PMB", "id": 1, "batchsize": 5,
     "srrkey": "aaaa"
   }})";
  // Make sure the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  mojom::TrustTokenKeyCommitmentResultPtr result =
      TrustTokenKeyCommitmentParser().Parse(input);
  ASSERT_TRUE(result);
  ASSERT_TRUE(result->id);
  EXPECT_EQ(result->id, 1);
}

TEST(TrustTokenKeyCommitmentParser, RejectsMissingID) {
  std::string input =
      R"({ "PrivateStateTokenV1PMB": {
     "protocol_version": "PrivateStateTokenV1PMB", "batchsize": 5,
     "srrkey": "aaaa"
   }})";
  // Make sure the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  mojom::TrustTokenKeyCommitmentResultPtr result =
      TrustTokenKeyCommitmentParser().Parse(input);
  EXPECT_FALSE(result);
}

TEST(TrustTokenKeyCommitmentParser, RejectsTypeUnsafeID) {
  std::string input =
      R"({ "PrivateStateTokenV1PMB": {
     "protocol_version": "PrivateStateTokenV1PMB", "id": "foo",
     "srrkey": "aaaa",
     "batchsize": 5
   }})";
  // Make sure the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  mojom::TrustTokenKeyCommitmentResultPtr result =
      TrustTokenKeyCommitmentParser().Parse(input);
  EXPECT_FALSE(result);
}

TEST(TrustTokenKeyCommitmentParserMultipleIssuers, InvalidJson) {
  std::string input = "";
  ASSERT_FALSE(
      base::JSONReader::Read(input));  // Make sure it's really not valid JSON.

  auto result = TrustTokenKeyCommitmentParser().ParseMultipleIssuers(input);
  EXPECT_FALSE(result);
}

TEST(TrustTokenKeyCommitmentParserMultipleIssuers, NotADictionary) {
  std::string input = "3";
  // Make sure the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  auto result = TrustTokenKeyCommitmentParser().ParseMultipleIssuers(input);
  EXPECT_FALSE(result);
}

TEST(TrustTokenKeyCommitmentParserMultipleIssuers, Empty) {
  std::string input = "{}";

  // Make sure the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  auto result = TrustTokenKeyCommitmentParser().ParseMultipleIssuers(input);
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->empty());
}

TEST(TrustTokenKeyCommitmentParserMultipleIssuers, UnsuitableKey) {
  // Test that a key with an unsuitable Trust Tokens origin gets skipped.
  std::string input =
      R"( { "http://insecure.example/":
             { "protocol_version": "PrivateStateTokenV1PMB", "id": 1,
               "batchsize": 5
                 } } )";

  // Make sure the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  auto result = TrustTokenKeyCommitmentParser().ParseMultipleIssuers(input);
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->empty());
}

TEST(TrustTokenKeyCommitmentParserMultipleIssuers, SuitableKeyInvalidValue) {
  // Test that a key-value pair with a malformed value gets skipped.
  std::string input =
      R"( { "https://insecure.example/":
              "not a valid encoding of a key commitment result" } )";

  // Make sure the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  auto result = TrustTokenKeyCommitmentParser().ParseMultipleIssuers(input);
  ASSERT_TRUE(result);
  EXPECT_TRUE(result->empty());
}

TEST(TrustTokenKeyCommitmentParserMultipleIssuers, SingleIssuer) {
  std::string input =
      R"( { "https://issuer.example/": {  "PrivateStateTokenV1PMB": {
              "protocol_version": "PrivateStateTokenV1PMB",
              "id": 1, "batchsize": 5
              }} } )";

  // Make sure the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  TrustTokenKeyCommitmentParser parser;

  auto result = parser.ParseMultipleIssuers(input);
  ASSERT_TRUE(result);
  auto issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.example"));
  ASSERT_TRUE(result->count(issuer));
  EXPECT_TRUE(
      mojo::Equals(result->at(issuer), parser.Parse(
                                           R"({  "PrivateStateTokenV1PMB": {
             "protocol_version": "PrivateStateTokenV1PMB",
             "id": 1, "batchsize": 5 }})")));
}

TEST(TrustTokenKeyCommitmentParserMultipleIssuers, DuplicateIssuer) {
  std::string input =
      R"( { "https://issuer.example/": {  "PrivateStateTokenV1PMB": {
            "protocol_version": "PrivateStateTokenV1PMB", "id": 1,
            "batchsize": 5 }},
    "https://other.example/": {  "PrivateStateTokenV1PMB": {
             "protocol_version": "PrivateStateTokenV1PMB", "id": 1,
             "batchsize": 5 }},
    "https://issuer.example/this-is-really-the-same-issuer-as-the-first-entry":
      { "PrivateStateTokenV1PMB": {
        "protocol_version": "PrivateStateTokenV1PMB", "id": 1,
        "batchsize": 3 }}
    } )";

  // Make sure the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  TrustTokenKeyCommitmentParser parser;

  auto result = parser.ParseMultipleIssuers(input);
  ASSERT_TRUE(result);

  // The result should have been deduplicated. Moreover, the entry with the
  // largest issuer JSON string lexicographically should win.
  ASSERT_EQ(result->size(), 2u);

  auto issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.example"));
  ASSERT_TRUE(result->count(issuer));
  EXPECT_TRUE(
      mojo::Equals(result->at(issuer), parser.Parse(
                                           R"({ "PrivateStateTokenV1PMB": {
        "protocol_version": "PrivateStateTokenV1PMB", "id": 1, "batchsize": 3
        }})")));
}

TEST(TrustTokenKeyCommitmentParserMultipleIssuers, DuplicateIssuerFirstWins) {
  // This is the same as DuplicateIssuer, except this time the first entry
  // position-wise is the longest lexicographically, so deduplication should
  // favor it.

  std::string input =
      R"( {
    "https://issuer.example/longer": {  "PrivateStateTokenV1PMB": {
      "protocol_version": "PrivateStateTokenV1PMB", "id": 1, "batchsize": 5 }},
    "https://other.example/": {  "PrivateStateTokenV1PMB": {
      "protocol_version": "PrivateStateTokenV1PMB", "id": 1, "batchsize": 5 }},
    "https://issuer.example/":
      { "PrivateStateTokenV1PMB": {
        "protocol_version": "PrivateStateTokenV1PMB", "id": 1,
        "batchsize": 3 }
    }} )";

  // Make sure the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  TrustTokenKeyCommitmentParser parser;

  auto result = parser.ParseMultipleIssuers(input);
  ASSERT_TRUE(result);

  // The result should have been deduplicated. Moreover, the entry with the
  // largest issuer JSON string lexicographically should win.
  ASSERT_EQ(result->size(), 2u);

  auto issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.example"));
  ASSERT_TRUE(result->count(issuer));
  EXPECT_TRUE(
      mojo::Equals(result->at(issuer), parser.Parse(
                                           R"({ "PrivateStateTokenV1PMB": {
               "protocol_version": "PrivateStateTokenV1PMB",
               "id": 1, "batchsize": 5 }})")));
}

TEST(TrustTokenKeyCommitmentParserMultipleIssuers,
     MixOfSuitableAndUnsuitableIssuers) {
  std::string input = R"( {
    "https://issuer.example/": { "PrivateStateTokenV1PMB": {
      "protocol_version": "PrivateStateTokenV1PMB", "id": 1, "batchsize": 5 }},
    "http://insecure.example": { "PrivateStateTokenV1PMB": {
      "protocol_version": "PrivateStateTokenV1PMB",
      "id": 1, "batchsize": 5 } }} )";

  // Make sure the input is actually valid JSON.
  ASSERT_TRUE(base::JSONReader::Read(input));

  TrustTokenKeyCommitmentParser parser;

  auto result = parser.ParseMultipleIssuers(input);
  ASSERT_TRUE(result);
  EXPECT_EQ(result->size(), 1u);

  auto issuer =
      *SuitableTrustTokenOrigin::Create(GURL("https://issuer.example"));
  ASSERT_TRUE(result->count(issuer));
  EXPECT_TRUE(
      mojo::Equals(result->at(issuer), parser.Parse(
                                           R"({  "PrivateStateTokenV1PMB": {
        "protocol_version": "PrivateStateTokenV1PMB",
        "id": 1, "batchsize": 5}})")));
}

}  // namespace network
