
// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/identity_digest.h"

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
    {IntegrityAlgorithm::kSha384,
     "/b2OdaZ/KfcBpOBAOF4uI5hjA+oQI5IRr5B/y7g1eLPkF8txzmRu/QgZ3YwIjeG9"},
    {IntegrityAlgorithm::kSha512,
     "MJ7MSJwS1utMxA9QyQLytNDtd+5RGnx6m808qG1M2G+"
     "YndNbxf9JlnDaNCVbRbDP2DDoH2Bdz33FVC6TrpzXbw=="},
};
static const std::map<IntegrityAlgorithm, const char*> kDlrowOllehs = {
    {IntegrityAlgorithm::kSha256,
     "vT+a3uWsoxRxVJEINKfH4XZpLqsneOzhFVY98Y3iIz0="},
    {IntegrityAlgorithm::kSha384,
     "rueKXz5kdtdmTpc6NbS9fCqr7z8h2mjNs43K9WUglTsZPJzKSUpR87dLs/FNemRN"},
    {IntegrityAlgorithm::kSha512,
     "N/"
     "peuevAy3l8KpS0bB6VTS8vc0fdAvjBJKYjVo2xb6sB6LpDfY6YlrXkWeeXGrP07UXDXEu1K3+"
     "SaUqMNjEkxQ=="},
};

String IntegrityAlgorithmToDictionaryName(IntegrityAlgorithm algorithm) {
  switch (algorithm) {
    case IntegrityAlgorithm::kSha256:
      return "sha-256";
    case IntegrityAlgorithm::kSha384:
      return "sha-384";
    case IntegrityAlgorithm::kSha512:
      return "sha-512";
    case IntegrityAlgorithm::kEd25519:
      NOTREACHED();
  }
}

}  // namespace

TEST(IdentityDigestParserTest, NoHeader) {
  HTTPHeaderMap no_identity_digest;
  auto result = IdentityDigest::Create(no_identity_digest);
  EXPECT_FALSE(result.has_value());
}

TEST(IdentityDigestParserTest, MalformedHeader) {
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
      "sha-384=:YQ==:",
      "sha-512=:YQ==:",
  };

  for (const char* test : cases) {
    SCOPED_TRACE(testing::Message() << "Header value: `" << test << "`");
    HTTPHeaderMap headers;
    headers.Set(http_names::kIdentityDigest, AtomicString(test));

    // As these are malformed headers, we expect parsing to return std::nullopt.
    auto result = IdentityDigest::Create(headers);
    EXPECT_FALSE(result.has_value());
  }
}

TEST(IdentityDigestParserTest, WellFormedHeaderWithSingleDigest) {
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

      // SHA-384
      //
      // One well-formed digest.
      {"sha-384=:/b2OdaZ/KfcBpOBAOF4uI5hjA+oQI5IRr5B/y7g1eLPkF8txzmRu/"
       "QgZ3YwIjeG9:",
       IntegrityAlgorithm::kSha384},
      // Dictionary parsing takes the last value associated with a key.
      {"sha-384=:YQ==:, "
       "sha-384=:/b2OdaZ/KfcBpOBAOF4uI5hjA+oQI5IRr5B/y7g1eLPkF8txzmRu/"
       "QgZ3YwIjeG9:",
       IntegrityAlgorithm::kSha384},
      // Unknown, Known => Known
      {"unknown-key=:YQ==:, "
       "sha-384=:/b2OdaZ/KfcBpOBAOF4uI5hjA+oQI5IRr5B/y7g1eLPkF8txzmRu/"
       "QgZ3YwIjeG9:",
       IntegrityAlgorithm::kSha384},
      // Known, Unknown => Known
      {"sha-384=:/b2OdaZ/KfcBpOBAOF4uI5hjA+oQI5IRr5B/y7g1eLPkF8txzmRu/"
       "QgZ3YwIjeG9:, unknown-key=:YQ==:",
       IntegrityAlgorithm::kSha384},

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
    headers.Set(http_names::kIdentityDigest, AtomicString(test.header));

    IntegrityMetadata expected;
    expected.SetAlgorithm(test.alg);
    expected.SetDigest(kHelloWorlds.at(test.alg));

    auto result = IdentityDigest::Create(headers);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(1u, result->digests().size());
    EXPECT_TRUE(result->digests().Contains(expected.ToPair()));
  }
}

TEST(IdentityDigestParserTest, MultipleDigests) {
  struct {
    const char* header;
    std::vector<IntegrityAlgorithm> alg;
  } cases[] = {
      // Two digests.
      {"sha-256=:uU0nuZNNPgilLlLX2n2r+sSE7+N6U4DukIj3rOLvzek=:,"
       "sha-384=:/b2OdaZ/KfcBpOBAOF4uI5hjA+oQI5IRr5B/y7g1eLPkF8txzmRu/"
       "QgZ3YwIjeG9:",
       {IntegrityAlgorithm::kSha256, IntegrityAlgorithm::kSha384}},
      {"sha-384=:/b2OdaZ/KfcBpOBAOF4uI5hjA+oQI5IRr5B/y7g1eLPkF8txzmRu/"
       "QgZ3YwIjeG9:,"
       "sha-512=:MJ7MSJwS1utMxA9QyQLytNDtd+5RGnx6m808qG1M2G+"
       "YndNbxf9JlnDaNCVbRbDP2DDoH2Bdz33FVC6TrpzXbw==:",
       {IntegrityAlgorithm::kSha384, IntegrityAlgorithm::kSha512}},
      {"sha-256=:uU0nuZNNPgilLlLX2n2r+sSE7+N6U4DukIj3rOLvzek=:,"
       "sha-512=:MJ7MSJwS1utMxA9QyQLytNDtd+5RGnx6m808qG1M2G+"
       "YndNbxf9JlnDaNCVbRbDP2DDoH2Bdz33FVC6TrpzXbw==:",
       {IntegrityAlgorithm::kSha256, IntegrityAlgorithm::kSha512}},

      // Two digests + unknown => Two digests
      {"sha-256=:uU0nuZNNPgilLlLX2n2r+sSE7+N6U4DukIj3rOLvzek=:,"
       "sha-384=:/b2OdaZ/KfcBpOBAOF4uI5hjA+oQI5IRr5B/y7g1eLPkF8txzmRu/"
       "QgZ3YwIjeG9:,"
       "unknown=:YQ==:",
       {IntegrityAlgorithm::kSha256, IntegrityAlgorithm::kSha384}},
      {"sha-384=:/b2OdaZ/KfcBpOBAOF4uI5hjA+oQI5IRr5B/y7g1eLPkF8txzmRu/"
       "QgZ3YwIjeG9:,"
       "sha-512=:MJ7MSJwS1utMxA9QyQLytNDtd+5RGnx6m808qG1M2G+"
       "YndNbxf9JlnDaNCVbRbDP2DDoH2Bdz33FVC6TrpzXbw==:,"
       "unknown=:YQ==:",
       {IntegrityAlgorithm::kSha384, IntegrityAlgorithm::kSha512}},
      {"sha-256=:uU0nuZNNPgilLlLX2n2r+sSE7+N6U4DukIj3rOLvzek=:,"
       "sha-512=:MJ7MSJwS1utMxA9QyQLytNDtd+5RGnx6m808qG1M2G+"
       "YndNbxf9JlnDaNCVbRbDP2DDoH2Bdz33FVC6TrpzXbw==:,"
       "unknown=:YQ==:",
       {IntegrityAlgorithm::kSha256, IntegrityAlgorithm::kSha512}},

      // Two digests + malformed => Two digests
      {"sha-256=:uU0nuZNNPgilLlLX2n2r+sSE7+N6U4DukIj3rOLvzek=:,"
       "sha-384=:/b2OdaZ/KfcBpOBAOF4uI5hjA+oQI5IRr5B/y7g1eLPkF8txzmRu/"
       "QgZ3YwIjeG9:,"
       "sha-512=:YQ==:",
       {IntegrityAlgorithm::kSha256, IntegrityAlgorithm::kSha384}},
      {"sha-384=:/b2OdaZ/KfcBpOBAOF4uI5hjA+oQI5IRr5B/y7g1eLPkF8txzmRu/"
       "QgZ3YwIjeG9:,"
       "sha-512=:MJ7MSJwS1utMxA9QyQLytNDtd+5RGnx6m808qG1M2G+"
       "YndNbxf9JlnDaNCVbRbDP2DDoH2Bdz33FVC6TrpzXbw==:,"
       "sha-256=:YQ==:",
       {IntegrityAlgorithm::kSha384, IntegrityAlgorithm::kSha512}},
      {"sha-256=:uU0nuZNNPgilLlLX2n2r+sSE7+N6U4DukIj3rOLvzek=:,"
       "sha-512=:MJ7MSJwS1utMxA9QyQLytNDtd+5RGnx6m808qG1M2G+"
       "YndNbxf9JlnDaNCVbRbDP2DDoH2Bdz33FVC6TrpzXbw==:,"
       "sha-384=:YQ==:",
       {IntegrityAlgorithm::kSha256, IntegrityAlgorithm::kSha512}},

      // Three digests
      {"sha-256=:uU0nuZNNPgilLlLX2n2r+sSE7+N6U4DukIj3rOLvzek=:,"
       "sha-384=:/b2OdaZ/KfcBpOBAOF4uI5hjA+oQI5IRr5B/y7g1eLPkF8txzmRu/"
       "QgZ3YwIjeG9:,"
       "sha-512=:MJ7MSJwS1utMxA9QyQLytNDtd+5RGnx6m808qG1M2G+"
       "YndNbxf9JlnDaNCVbRbDP2DDoH2Bdz33FVC6TrpzXbw==:",
       {IntegrityAlgorithm::kSha256, IntegrityAlgorithm::kSha384,
        IntegrityAlgorithm::kSha512}},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << "Header value: `" << test.header << "`");
    HTTPHeaderMap headers;
    headers.Set(http_names::kIdentityDigest, AtomicString(test.header));

    auto result = IdentityDigest::Create(headers);
    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(test.alg.size(), result->digests().size());

    for (const auto& algorithm : test.alg) {
      IntegrityMetadata expected;
      expected.SetAlgorithm(algorithm);
      expected.SetDigest(kHelloWorlds.at(algorithm));
      EXPECT_TRUE(result->digests().Contains(expected.ToPair()));
    }
  }
}

TEST(IdentityDigestMatchingTest, MatchingSingleDigests) {
  for (const auto& [alg, digest] : kHelloWorlds) {
    SCOPED_TRACE(testing::Message() << IntegrityAlgorithmToDictionaryName(alg));

    String test = IntegrityAlgorithmToDictionaryName(alg) + "=:" + digest + ":";

    HTTPHeaderMap headers;
    headers.Set(http_names::kIdentityDigest, AtomicString(test));

    auto identity_digest = IdentityDigest::Create(headers);
    ASSERT_TRUE(identity_digest.has_value());

    WTF::SegmentedBuffer buffer;
    buffer.Append(kHelloWorld);
    EXPECT_TRUE(identity_digest->DoesMatch(&buffer));

    buffer.Clear();
    buffer.Append(kDlrowOlleh);
    EXPECT_FALSE(identity_digest->DoesMatch(&buffer));
  }
}

TEST(IdentityDigestMatchingTest, MatchingMultipleDigests) {
  std::vector<IntegrityAlgorithm> cases[] = {
      {IntegrityAlgorithm::kSha256, IntegrityAlgorithm::kSha384},
      {IntegrityAlgorithm::kSha256, IntegrityAlgorithm::kSha384},
      {IntegrityAlgorithm::kSha384, IntegrityAlgorithm::kSha256},
      {IntegrityAlgorithm::kSha384, IntegrityAlgorithm::kSha512},
      {IntegrityAlgorithm::kSha512, IntegrityAlgorithm::kSha256},
      {IntegrityAlgorithm::kSha512, IntegrityAlgorithm::kSha384},
      {IntegrityAlgorithm::kSha256, IntegrityAlgorithm::kSha384,
       IntegrityAlgorithm::kSha512},
      {IntegrityAlgorithm::kSha256, IntegrityAlgorithm::kSha512,
       IntegrityAlgorithm::kSha384},
      {IntegrityAlgorithm::kSha384, IntegrityAlgorithm::kSha256,
       IntegrityAlgorithm::kSha512},
      {IntegrityAlgorithm::kSha384, IntegrityAlgorithm::kSha512,
       IntegrityAlgorithm::kSha256},
      {IntegrityAlgorithm::kSha512, IntegrityAlgorithm::kSha256,
       IntegrityAlgorithm::kSha384},
      {IntegrityAlgorithm::kSha512, IntegrityAlgorithm::kSha384,
       IntegrityAlgorithm::kSha256},
  };

  for (const auto& test : cases) {
    StringBuilder header_value;
    for (const auto& alg : test) {
      if (!header_value.empty()) {
        header_value.Append(',');
      }
      header_value.Append(IntegrityAlgorithmToDictionaryName(alg));
      header_value.Append("=:");
      header_value.Append(kHelloWorlds.at(alg));
      header_value.Append(':');
    }
    SCOPED_TRACE(testing::Message()
                 << "Header value: `" << header_value.ToString() << "`");

    HTTPHeaderMap headers;
    headers.Set(http_names::kIdentityDigest,
                AtomicString(header_value.ToString()));

    auto identity_digest = IdentityDigest::Create(headers);
    ASSERT_TRUE(identity_digest.has_value());

    WTF::SegmentedBuffer buffer;
    buffer.Append(kHelloWorld);
    EXPECT_TRUE(identity_digest->DoesMatch(&buffer));

    buffer.Clear();
    buffer.Append(kDlrowOlleh);
    EXPECT_FALSE(identity_digest->DoesMatch(&buffer));
  }
}

TEST(IdentityDigestMatchingTest, OneMatchingOneMismatching) {
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
      headers.Set(http_names::kIdentityDigest, AtomicString(test));

      auto identity_digest = IdentityDigest::Create(headers);
      ASSERT_TRUE(identity_digest.has_value());

      WTF::SegmentedBuffer buffer;
      buffer.Append(kHelloWorld);
      EXPECT_FALSE(identity_digest->DoesMatch(&buffer));

      buffer.Clear();
      buffer.Append(kDlrowOlleh);
      EXPECT_FALSE(identity_digest->DoesMatch(&buffer));
    }
  }
}

}  // namespace blink
