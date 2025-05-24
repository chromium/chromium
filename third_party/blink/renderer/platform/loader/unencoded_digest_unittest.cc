// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/unencoded_digest.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/renderer/platform/loader/fetch/integrity_metadata.h"
#include "third_party/blink/renderer/platform/network/http_header_map.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/wtf/shared_buffer.h"
#include "third_party/blink/renderer/platform/wtf/text/atomic_string.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"

namespace blink {

namespace {

static const base::span<const char, 11u> kHelloWorld =
    base::span_from_cstring("hello world");
static const base::span<const char, 11u> kDlrowOlleh =
    base::span_from_cstring("dlrow olleh");

// Known hashes of the exciting strings above.
static const std::map<IntegrityAlgorithm, const char*> kHelloWorlds = {
    {IntegrityAlgorithm::kSha256,
     "uU0nuZNNPgilLlLX2n2r+sSE7+N6U4DukIj3rOLvzek="},
    {IntegrityAlgorithm::kSha512,
     "MJ7MSJwS1utMxA9QyQLytNDtd+5RGnx6m808qG1M2G+"
     "YndNbxf9JlnDaNCVbRbDP2DDoH2Bdz33FVC6TrpzXbw=="},
};
static const std::map<IntegrityAlgorithm, const char*> kDlrowOllehs = {
    {IntegrityAlgorithm::kSha256,
     "vT+a3uWsoxRxVJEINKfH4XZpLqsneOzhFVY98Y3iIz0="},
    {IntegrityAlgorithm::kSha512,
     "N/"
     "peuevAy3l8KpS0bB6VTS8vc0fdAvjBJKYjVo2xb6sB6LpDfY6YlrXkWeeXGrP07UXDXEu1K3+"
     "SaUqMNjEkxQ=="},
};

String IntegrityAlgorithmToDictionaryName(IntegrityAlgorithm algorithm) {
  switch (algorithm) {
    case IntegrityAlgorithm::kSha256:
      return "sha-256";
    case IntegrityAlgorithm::kSha512:
      return "sha-512";
    case IntegrityAlgorithm::kSha384:
    case IntegrityAlgorithm::kEd25519:
      NOTREACHED();
  }
}

}  // namespace

TEST(UnencodedDigestParserTest, NoHeader) {
  HTTPHeaderMap no_unencoded_digest;
  auto result = UnencodedDigest::Create(no_unencoded_digest);
  EXPECT_FALSE(result.has_value());
}

TEST(UnencodedDigestParserTest, MalformedHeader) {
  const char* cases[] = {
      // Non-dictionaries
      "",
      "1",
      "1.1",
      "\"string\"",
      "token",
      ":lS/LFS0xbMKoQ0JWBZySc9ChRIZMbAuWO69kAVCb12k=:",
      "?0",
      "@12345",
      "%\"Display string\"",
      "A, list, of, tokens",
      "(inner list)",

      // Bad Dictionaries
      //
      // (unknown key, unrecognized type)
      "key=",
      "key=1",
      "key=1.1",
      "key=\"string\"",
      "key=token",
      "key=?0",
      "key=@12345",
      "key=%\"Display string\"",
      "key=(inner list of tokens)",

      // (unknown key, recognized type)
      "key=:lS/LFS0xbMKoQ0JWBZySc9ChRIZMbAuWO69kAVCb12k=:",

      // Special case of unknown key: `sha-384` isn't a valid digest header
      // hashing algorithm:
      //
      // https://www.iana.org/assignments/http-digest-hash-alg/http-digest-hash-alg.xhtml
      ("sha-384=:/b2OdaZ/KfcBpOBAOF4uI5hjA+oQI5IRr5B/y7g1eLPkF8txzmRu/"
       "QgZ3YwIjeG9:"),

      // (known key, unrecognized type)
      "sha-256=",
      "sha-256=1",
      "sha-256=1.1",
      "sha-256=\"string\"",
      "sha-256=token",
      "sha-256=?0",
      "sha-256=@12345",
      "sha-256=%\"Display string\"",
      "sha-256=(inner list of tokens)",

      // (known key, invalid digest)
      "sha-256=:YQ==:",
      "sha-512=:YQ==:",
  };

  for (const char* test : cases) {
    SCOPED_TRACE(testing::Message() << "Header value: `" << test << "`");
    HTTPHeaderMap headers;
    headers.Set(http_names::kUnencodedDigest, AtomicString(test));

    // As these are malformed headers, we expect parsing to return std::nullopt.
    auto result = UnencodedDigest::Create(headers);
    EXPECT_FALSE(result.has_value());
  }
}

TEST(UnencodedDigestParserTest, WellFormedHeaderWithSingleDigest) {
  struct {
    const char* header;
    IntegrityAlgorithm alg;
  } cases[] = {
      // SHA-256
      //
      // One well-formed digest.
      {"sha-256=:uU0nuZNNPgilLlLX2n2r+sSE7+N6U4DukIj3rOLvzek=:",
       IntegrityAlgorithm::kSha256},
      // Dictionary parsing takes the last value associated with a key.
      {"sha-256=:YQ==:, sha-256=:uU0nuZNNPgilLlLX2n2r+sSE7+N6U4DukIj3rOLvzek=:",
       IntegrityAlgorithm::kSha256},
      // Unknown, Known => Known
      {"unknown-key=:YQ==:, "
       "sha-256=:uU0nuZNNPgilLlLX2n2r+sSE7+N6U4DukIj3rOLvzek=:",
       IntegrityAlgorithm::kSha256},
      // Known, Unknown => Known
      {"sha-256=:uU0nuZNNPgilLlLX2n2r+sSE7+N6U4DukIj3rOLvzek=:, "
       "unknown-key=:YQ==:",
       IntegrityAlgorithm::kSha256},

      // SHA-512
      //
      // One well-formed digest.
      {"sha-512=:MJ7MSJwS1utMxA9QyQLytNDtd+5RGnx6m808qG1M2G+"
       "YndNbxf9JlnDaNCVbRbDP2DDoH2Bdz33FVC6TrpzXbw==:",
       IntegrityAlgorithm::kSha512},
      // Dictionary parsing takes the last value associated with a key.
      {"sha-512=:YQ==:, "
       "sha-512=:MJ7MSJwS1utMxA9QyQLytNDtd+5RGnx6m808qG1M2G+"
       "YndNbxf9JlnDaNCVbRbDP2DDoH2Bdz33FVC6TrpzXbw==:",
       IntegrityAlgorithm::kSha512},
      // Unknown, Known => Known
      {"unknown-key=:YQ==:, "
       "sha-512=:MJ7MSJwS1utMxA9QyQLytNDtd+5RGnx6m808qG1M2G+"
       "YndNbxf9JlnDaNCVbRbDP2DDoH2Bdz33FVC6TrpzXbw==:",
       IntegrityAlgorithm::kSha512},
      // Known, Unknown => Known
      {"sha-512=:MJ7MSJwS1utMxA9QyQLytNDtd+5RGnx6m808qG1M2G+"
       "YndNbxf9JlnDaNCVbRbDP2DDoH2Bdz33FVC6TrpzXbw==:, unknown-key=:YQ==:",
       IntegrityAlgorithm::kSha512},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << "Header value: `" << test.header << "`");
    HTTPHeaderMap headers;
    headers.Set(http_names::kUnencodedDigest, AtomicString(test.header));

    IntegrityMetadata expected;
    expected.algorithm = test.alg;
    expected.digest = kHelloWorlds.at(test.alg);

    auto result = UnencodedDigest::Create(headers);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(1u, result->digests().size());
    EXPECT_TRUE(result->digests().Contains(expected));
  }
}

TEST(UnencodedDigestParserTest, MultipleDigests) {
  struct {
    const char* header;
    std::vector<IntegrityAlgorithm> alg;
  } cases[] = {
      // Two digests.
      {"sha-256=:uU0nuZNNPgilLlLX2n2r+sSE7+N6U4DukIj3rOLvzek=:,"
       "sha-512=:MJ7MSJwS1utMxA9QyQLytNDtd+5RGnx6m808qG1M2G+"
       "YndNbxf9JlnDaNCVbRbDP2DDoH2Bdz33FVC6TrpzXbw==:",
       {IntegrityAlgorithm::kSha256, IntegrityAlgorithm::kSha512}},

      // Two digests + unknown => Two digests
      {"sha-256=:uU0nuZNNPgilLlLX2n2r+sSE7+N6U4DukIj3rOLvzek=:,"
       "sha-512=:MJ7MSJwS1utMxA9QyQLytNDtd+5RGnx6m808qG1M2G+"
       "YndNbxf9JlnDaNCVbRbDP2DDoH2Bdz33FVC6TrpzXbw==:,"
       "unknown=:YQ==:",
       {IntegrityAlgorithm::kSha256, IntegrityAlgorithm::kSha512}},

      // Two digests + malformed => Two digests
      {"sha-256=:uU0nuZNNPgilLlLX2n2r+sSE7+N6U4DukIj3rOLvzek=:,"
       "sha-512=:MJ7MSJwS1utMxA9QyQLytNDtd+5RGnx6m808qG1M2G+"
       "YndNbxf9JlnDaNCVbRbDP2DDoH2Bdz33FVC6TrpzXbw==:,"
       "sha-384=:YQ==:",
       {IntegrityAlgorithm::kSha256, IntegrityAlgorithm::kSha512}},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << "Header value: `" << test.header << "`");
    HTTPHeaderMap headers;
    headers.Set(http_names::kUnencodedDigest, AtomicString(test.header));

    auto result = UnencodedDigest::Create(headers);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(test.alg.size(), result->digests().size());

    for (const auto& algorithm : test.alg) {
      IntegrityMetadata expected;
      expected.algorithm = algorithm;
      expected.digest = kHelloWorlds.at(algorithm);
      EXPECT_TRUE(result->digests().Contains(expected));
    }
  }
}

TEST(UnencodedDigestMatchingTest, MatchingSingleDigests) {
  for (const auto& [alg, digest] : kHelloWorlds) {
    SCOPED_TRACE(testing::Message() << IntegrityAlgorithmToDictionaryName(alg));

    String test = IntegrityAlgorithmToDictionaryName(alg) + "=:" + digest + ":";

    HTTPHeaderMap headers;
    headers.Set(http_names::kUnencodedDigest, AtomicString(test));

    auto unencoded_digest = UnencodedDigest::Create(headers);
    ASSERT_TRUE(unencoded_digest.has_value());

    WTF::SegmentedBuffer buffer;
    buffer.Append(kHelloWorld);
    EXPECT_TRUE(unencoded_digest->DoesMatch(&buffer));

    buffer.Clear();
    buffer.Append(kDlrowOlleh);
    EXPECT_FALSE(unencoded_digest->DoesMatch(&buffer));
  }
}

TEST(UnencodedDigestMatchingTest, OneMatchingOneMismatching) {
  // Combine a "hello world" hash with a mirror-world "dlrow olleh" twin. We'll
  // then verify that the resulting digest doesn't match either string.
  for (const auto& [alg, digest] : kHelloWorlds) {
    for (const auto& [gla, tsegid] : kDlrowOllehs) {
      if (alg == gla) {
        // Dictionaries take the last entry with a given name, so skip algorithm
        // matches for this test.
        continue;
      }
      String test = IntegrityAlgorithmToDictionaryName(alg) + "=:" + digest +
                    ":," + IntegrityAlgorithmToDictionaryName(gla) +
                    "=:" + tsegid + ":";
      SCOPED_TRACE(testing::Message() << test);

      HTTPHeaderMap headers;
      headers.Set(http_names::kUnencodedDigest, AtomicString(test));

      auto unencoded_digest = UnencodedDigest::Create(headers);
      ASSERT_TRUE(unencoded_digest.has_value());

      WTF::SegmentedBuffer buffer;
      buffer.Append(kHelloWorld);
      EXPECT_FALSE(unencoded_digest->DoesMatch(&buffer));

      buffer.Clear();
      buffer.Append(kDlrowOlleh);
      EXPECT_FALSE(unencoded_digest->DoesMatch(&buffer));
    }
  }
}

}  // namespace blink
