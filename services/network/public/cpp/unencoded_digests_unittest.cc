// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/public/cpp/unencoded_digests.h"

#include "base/base64.h"
#include "net/http/http_response_headers.h"
#include "services/network/public/cpp/integrity_metadata.h"
#include "services/network/public/mojom/integrity_algorithm.mojom.h"
#include "services/network/public/mojom/unencoded_digest.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace network {

namespace {

// Known hashes of "hello world":
static const std::map<mojom::IntegrityAlgorithm, const char*> kHelloWorlds = {
    {mojom::IntegrityAlgorithm::kSha256,
     "uU0nuZNNPgilLlLX2n2r+sSE7+N6U4DukIj3rOLvzek="},
    {mojom::IntegrityAlgorithm::kSha512,
     "MJ7MSJwS1utMxA9QyQLytNDtd+5RGnx6m808qG1M2G+"
     "YndNbxf9JlnDaNCVbRbDP2DDoH2Bdz33FVC6TrpzXbw=="},
};

IntegrityMetadata CreateTestMetadata(mojom::IntegrityAlgorithm alg) {
  std::optional<IntegrityMetadata> metadata =
      IntegrityMetadata::CreateFromBase64(alg, kHelloWorlds.at(alg));
  CHECK(metadata);
  return std::move(*metadata);
}

}  // namespace

class UnencodedDigestParserTest : public testing::Test {
 protected:
  UnencodedDigestParserTest() = default;

  scoped_refptr<net::HttpResponseHeaders> GetHeaders(const char* digest) {
    auto builder =
        net::HttpResponseHeaders::Builder(net::HttpVersion(1, 1), "200");
    if (digest) {
      builder.AddHeader("Unencoded-Digest", digest);
    }
    return builder.Build();
  }
};

TEST_F(UnencodedDigestParserTest, NoHeader) {
  auto headers = GetHeaders(/*digest=*/nullptr);
  mojom::UnencodedDigestsPtr result =
      ParseUnencodedDigestsFromHeaders(*headers);
  EXPECT_EQ(0u, result->digests.size());
  EXPECT_EQ(0u, result->issues.size());
}

TEST_F(UnencodedDigestParserTest, MalformedHeader) {
  struct {
    const char* digest;
    mojom::UnencodedDigestIssue expected_error;
  } cases[] = {
      // Malformed dictionaries:
      {"1", mojom::UnencodedDigestIssue::kMalformedDictionary},
      {"1.1", mojom::UnencodedDigestIssue::kMalformedDictionary},
      {"\"string\"", mojom::UnencodedDigestIssue::kMalformedDictionary},
      {":lS/LFS0xbMKoQ0JWBZySc9ChRIZMbAuWO69kAVCb12k=:",
       mojom::UnencodedDigestIssue::kMalformedDictionary},
      {"?0", mojom::UnencodedDigestIssue::kMalformedDictionary},
      {"@12345", mojom::UnencodedDigestIssue::kMalformedDictionary},
      {"%\"Display string\"",
       mojom::UnencodedDigestIssue::kMalformedDictionary},
      {"A, List, Of, Tokens",
       mojom::UnencodedDigestIssue::kMalformedDictionary},
      {"(inner list of tokens)",
       mojom::UnencodedDigestIssue::kMalformedDictionary},
      // These should be kUnknownAlgorithm, but we don't support dates or
      // display strings:
      {"token=@12345", mojom::UnencodedDigestIssue::kMalformedDictionary},
      {"token=%\"Display string\"",
       mojom::UnencodedDigestIssue::kMalformedDictionary},
      // These should likewise be kIncorrectDigestType.
      {"sha-256=@12345", mojom::UnencodedDigestIssue::kMalformedDictionary},
      {"sha-256=%\"Display string\"",
       mojom::UnencodedDigestIssue::kMalformedDictionary},
      // Structured field labels must be lower-case:
      {"SHA-256=:uU0nuZNNPgilLlLX2n2r+sSE7+N6U4DukIj3rOLvzek=:",
       mojom::UnencodedDigestIssue::kMalformedDictionary},

      // Unknown algorithm labels:
      {"token", mojom::UnencodedDigestIssue::kUnknownAlgorithm},
      {"token=1", mojom::UnencodedDigestIssue::kUnknownAlgorithm},
      {"token=1.1", mojom::UnencodedDigestIssue::kUnknownAlgorithm},
      {"token=\"string\"", mojom::UnencodedDigestIssue::kUnknownAlgorithm},
      {"token=?0", mojom::UnencodedDigestIssue::kUnknownAlgorithm},
      {"token=:uU0nuZNNPgilLlLX2n2r+sSE7+N6U4DukIj3rOLvzek=:",
       mojom::UnencodedDigestIssue::kUnknownAlgorithm},
      {"token=(inner list of token)",
       mojom::UnencodedDigestIssue::kUnknownAlgorithm},
      {"sha-384=:uU0nuZNNPgilLlLX2n2r+sSE7+N6U4DukIj3rOLvzek=:",
       mojom::UnencodedDigestIssue::kUnknownAlgorithm},

      // Known algorithm labels, incorrect type:
      {"sha-256", mojom::UnencodedDigestIssue::kIncorrectDigestType},
      {"sha-256=1", mojom::UnencodedDigestIssue::kIncorrectDigestType},
      {"sha-256=1.1", mojom::UnencodedDigestIssue::kIncorrectDigestType},
      {"sha-256=\"string\"", mojom::UnencodedDigestIssue::kIncorrectDigestType},
      {"sha-256=?0", mojom::UnencodedDigestIssue::kIncorrectDigestType},
      {"sha-256=(inner list of token)",
       mojom::UnencodedDigestIssue::kIncorrectDigestType},

      // Known algorithm labels, incorrect digest length:
      {"sha-256=:YQ==:", mojom::UnencodedDigestIssue::kIncorrectDigestLength},
      {"sha-512=:YQ==:", mojom::UnencodedDigestIssue::kIncorrectDigestLength},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << "Header value: `" << test.digest << "`");
    auto headers = GetHeaders(test.digest);
    mojom::UnencodedDigestsPtr result =
        ParseUnencodedDigestsFromHeaders(*headers);
    EXPECT_EQ(0u, result->digests.size());
    EXPECT_EQ(1u, result->issues.size());
    EXPECT_EQ(test.expected_error, result->issues[0]);
  }
}

TEST_F(UnencodedDigestParserTest, WellFormedHeaderWithSingleDigest) {
  struct {
    const char* digest;
    mojom::IntegrityAlgorithm alg;
    std::vector<mojom::UnencodedDigestIssue> errors;
  } cases[] = {
      // SHA-256
      //
      // One well-formed digest.
      {"sha-256=:uU0nuZNNPgilLlLX2n2r+sSE7+N6U4DukIj3rOLvzek=:",
       mojom::IntegrityAlgorithm::kSha256,
       {}},
      // Dictionary parsing takes the last value associated with a key.
      {"sha-256=:YQ==:, sha-256=:uU0nuZNNPgilLlLX2n2r+sSE7+N6U4DukIj3rOLvzek=:",
       mojom::IntegrityAlgorithm::kSha256,
       {}},
      // Unknown, Known => Known
      {"unknown-key=:YQ==:, "
       "sha-256=:uU0nuZNNPgilLlLX2n2r+sSE7+N6U4DukIj3rOLvzek=:",
       mojom::IntegrityAlgorithm::kSha256,
       {mojom::UnencodedDigestIssue::kUnknownAlgorithm}},
      // Known, Unknown => Known
      {"sha-256=:uU0nuZNNPgilLlLX2n2r+sSE7+N6U4DukIj3rOLvzek=:, "
       "unknown-key=:YQ==:",
       mojom::IntegrityAlgorithm::kSha256,
       {mojom::UnencodedDigestIssue::kUnknownAlgorithm}},

      // SHA-512
      //
      // One well-formed digest.
      {"sha-512=:MJ7MSJwS1utMxA9QyQLytNDtd+5RGnx6m808qG1M2G+"
       "YndNbxf9JlnDaNCVbRbDP2DDoH2Bdz33FVC6TrpzXbw==:",
       mojom::IntegrityAlgorithm::kSha512,
       {}},
      // Dictionary parsing takes the last value associated with a key.
      {"sha-512=:YQ==:, "
       "sha-512=:MJ7MSJwS1utMxA9QyQLytNDtd+5RGnx6m808qG1M2G+"
       "YndNbxf9JlnDaNCVbRbDP2DDoH2Bdz33FVC6TrpzXbw==:",
       mojom::IntegrityAlgorithm::kSha512,
       {}},
      // Unknown, Known => Known
      {"unknown-key=:YQ==:, "
       "sha-512=:MJ7MSJwS1utMxA9QyQLytNDtd+5RGnx6m808qG1M2G+"
       "YndNbxf9JlnDaNCVbRbDP2DDoH2Bdz33FVC6TrpzXbw==:",
       mojom::IntegrityAlgorithm::kSha512,
       {mojom::UnencodedDigestIssue::kUnknownAlgorithm}},
      // Known, Unknown => Known
      {"sha-512=:MJ7MSJwS1utMxA9QyQLytNDtd+5RGnx6m808qG1M2G+"
       "YndNbxf9JlnDaNCVbRbDP2DDoH2Bdz33FVC6TrpzXbw==:, unknown-key=:YQ==:",
       mojom::IntegrityAlgorithm::kSha512,
       {mojom::UnencodedDigestIssue::kUnknownAlgorithm}},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << "Header value: `" << test.digest << "`");

    auto headers = GetHeaders(test.digest);
    mojom::UnencodedDigestsPtr result =
        ParseUnencodedDigestsFromHeaders(*headers);

    ASSERT_EQ(1u, result->digests.size());
    EXPECT_EQ(test.errors, result->issues);
    EXPECT_EQ(CreateTestMetadata(test.alg), result->digests[0]);
  }
}

TEST_F(UnencodedDigestParserTest, MultipleDigests) {
  struct {
    const char* digest;
    std::vector<mojom::IntegrityAlgorithm> alg;
    std::vector<mojom::UnencodedDigestIssue> errors;
  } cases[] = {
      // Two digests.
      {"sha-256=:uU0nuZNNPgilLlLX2n2r+sSE7+N6U4DukIj3rOLvzek=:,"
       "sha-512=:MJ7MSJwS1utMxA9QyQLytNDtd+5RGnx6m808qG1M2G+"
       "YndNbxf9JlnDaNCVbRbDP2DDoH2Bdz33FVC6TrpzXbw==:",
       {mojom::IntegrityAlgorithm::kSha256, mojom::IntegrityAlgorithm::kSha512},
       {}},

      // Two digests + unknown => Two digests
      {"sha-256=:uU0nuZNNPgilLlLX2n2r+sSE7+N6U4DukIj3rOLvzek=:,"
       "sha-512=:MJ7MSJwS1utMxA9QyQLytNDtd+5RGnx6m808qG1M2G+"
       "YndNbxf9JlnDaNCVbRbDP2DDoH2Bdz33FVC6TrpzXbw==:,"
       "unknown=:YQ==:",
       {mojom::IntegrityAlgorithm::kSha256, mojom::IntegrityAlgorithm::kSha512},
       {mojom::UnencodedDigestIssue::kUnknownAlgorithm}},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message() << "Header value: `" << test.digest << "`");

    auto headers = GetHeaders(test.digest);
    mojom::UnencodedDigestsPtr result =
        ParseUnencodedDigestsFromHeaders(*headers);
    ASSERT_EQ(2u, result->digests.size());
    EXPECT_EQ(test.errors, result->issues);

    EXPECT_EQ(CreateTestMetadata(test.alg[0]), result->digests[0]);
    EXPECT_EQ(CreateTestMetadata(test.alg[1]), result->digests[1]);
  }
}

}  // namespace network
