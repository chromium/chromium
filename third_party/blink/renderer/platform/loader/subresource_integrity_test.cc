// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/platform/loader/subresource_integrity.h"

#include <algorithm>

#include "base/memory/scoped_refptr.h"
#include "base/strings/string_view_util.h"
#include "services/network/public/mojom/fetch_api.mojom-blink.h"
#include "services/network/public/mojom/integrity_algorithm.mojom-blink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/platform/crypto.h"
#include "third_party/blink/renderer/platform/feature_context.h"
#include "third_party/blink/renderer/platform/loader/fetch/integrity_metadata.h"
#include "third_party/blink/renderer/platform/loader/fetch/raw_resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_fetcher.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_scheduler.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_loader.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_response.h"
#include "third_party/blink/renderer/platform/loader/integrity_report.h"
#include "third_party/blink/renderer/platform/loader/testing/mock_fetch_context.h"
#include "third_party/blink/renderer/platform/network/http_names.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/testing/runtime_enabled_features_test_helpers.h"
#include "third_party/blink/renderer/platform/weborigin/kurl.h"
#include "third_party/blink/renderer/platform/weborigin/security_origin.h"
#include "third_party/blink/renderer/platform/wtf/text/base64.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

namespace blink {

namespace {

using network::mojom::FetchResponseType;
using network::mojom::RequestMode;

constexpr char kBasicScript[] = "alert('test');";
constexpr char kSha256Integrity[] =
    "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4=";
constexpr char kSha256IntegrityLenientSyntax[] =
    "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4=";
constexpr char kSha256IntegrityWithEmptyOption[] =
    "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4=?";
constexpr char kSha256IntegrityWithOption[] =
    "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4=?foo=bar";
constexpr char kSha256IntegrityWithOptions[] =
    "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4=?foo=bar?baz=foz";
constexpr char kSha256IntegrityWithMimeOption[] =
    "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4=?ct=application/"
    "javascript";
constexpr char kSha384Integrity[] =
    "sha384-nep3XpvhUxpCMOVXIFPecThAqdY_uVeiD4kXSqXpx0YJUWU4fTTaFgciTuZk7fmE";
constexpr char kSha512Integrity[] =
    "sha512-TXkJw18PqlVlEUXXjeXbGetop1TKB3wYQIp1_"
    "ihxCOFGUfG9TYOaA1MlkpTAqSV6yaevLO8Tj5pgH1JmZ--ItA==";
constexpr char kSha384IntegrityLabeledAs256[] =
    "sha256-nep3XpvhUxpCMOVXIFPecThAqdY_uVeiD4kXSqXpx0YJUWU4fTTaFgciTuZk7fmE";
constexpr char kSha256AndSha384Integrities[] =
    "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4= "
    "sha384-nep3XpvhUxpCMOVXIFPecThAqdY_uVeiD4kXSqXpx0YJUWU4fTTaFgciTuZk7fmE";
constexpr char kBadSha256AndGoodSha384Integrities[] =
    "sha256-deadbeef "
    "sha384-nep3XpvhUxpCMOVXIFPecThAqdY_uVeiD4kXSqXpx0YJUWU4fTTaFgciTuZk7fmE";
constexpr char kGoodSha256AndBadSha384Integrities[] =
    "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4= sha384-deadbeef";
constexpr char kBadSha256AndBadSha384Integrities[] =
    "sha256-deadbeef sha384-deadbeef";
constexpr char kUnsupportedHashFunctionIntegrity[] =
    "sha1-JfLW308qMPKfb4DaHpUBEESwuPc=";

}  // namespace

class SubresourceIntegrityTest : public testing::Test {
 public:
  SubresourceIntegrityTest()
      : sec_url("https://example.test:443"),
        insec_url("http://example.test:80"),
        context(MakeGarbageCollected<MockFetchContext>()) {}

 protected:
  network::IntegrityMetadata CreateIntegrityMetadata(
      const String& digest,
      IntegrityAlgorithm algorithm) {
    std::optional<network::IntegrityMetadata> expected =
        network::IntegrityMetadata::CreateFromBase64(algorithm, digest.Ascii());
    CHECK(expected);
    return *expected;
  }

  String AlgorithmToPrefix(IntegrityAlgorithm alg) {
    switch (alg) {
      case IntegrityAlgorithm::kSha256:
        return "sha256-";
      case IntegrityAlgorithm::kSha384:
        return "sha384-";
      case IntegrityAlgorithm::kSha512:
        return "sha512-";
      case IntegrityAlgorithm::kEd25519:
        return "ed25519-";
    }
  }

  void ExpectAlgorithm(const String& text,
                       IntegrityAlgorithm expected_algorithm) {
    StringUtf8Adaptor string_data(text);

    IntegrityAlgorithm algorithm;
    auto result = SubresourceIntegrity::ParseAttributeAlgorithm(
        base::as_string_view(string_data), nullptr, algorithm);
    ASSERT_TRUE(result.has_value());
    // All algorithm identifiers are currently 6 or 7 characters long.
    EXPECT_TRUE(result.value() == 6u || result.value() == 7u);
    EXPECT_EQ(expected_algorithm, algorithm);
  }

  void ExpectAlgorithmFailure(
      const String& text,
      SubresourceIntegrity::AlgorithmParseError expected_error) {
    StringUtf8Adaptor string_data(text);

    IntegrityAlgorithm algorithm;
    auto result = SubresourceIntegrity::ParseAttributeAlgorithm(
        base::as_string_view(string_data), nullptr, algorithm);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(expected_error, result.error());
  }

  void ExpectDigest(const String& text, const char* expected_digest) {
    StringUtf8Adaptor string_data(text);

    String digest;
    EXPECT_TRUE(SubresourceIntegrity::ParseDigest(
        base::as_string_view(string_data), digest));
    EXPECT_EQ(expected_digest, digest);
  }

  void ExpectDigestFailure(const String& text) {
    StringUtf8Adaptor string_data(text);

    String digest;
    EXPECT_FALSE(SubresourceIntegrity::ParseDigest(
        base::as_string_view(string_data), digest));
    EXPECT_TRUE(digest.empty());
  }

  void ExpectParse(const char* integrity_attribute,
                   const char* expected_digest,
                   IntegrityAlgorithm expected_algorithm) {
    IntegrityMetadataSet metadata_set;
    SubresourceIntegrity::ParseIntegrityAttribute(
        integrity_attribute, metadata_set, /*feature_context=*/nullptr);
    EXPECT_EQ(1u, metadata_set.hashes.size());
    if (metadata_set.hashes.size() > 0) {
      Vector<uint8_t> expected_binary_digest;
      ASSERT_TRUE(Base64Decode(expected_digest, expected_binary_digest));

      network::IntegrityMetadata expected(expected_algorithm,
                                          std::move(expected_binary_digest));
      EXPECT_EQ(expected, *metadata_set.hashes.begin());
    }
  }

  void ExpectParseMultipleHashes(
      const char* integrity_attribute,
      base::span<const IntegrityMetadata> expected_metadata) {
    IntegrityMetadataSet expected_metadata_set;
    for (const auto& expected : expected_metadata) {
      expected_metadata_set.Insert(std::move(expected));
    }
    IntegrityMetadataSet metadata_set;
    SubresourceIntegrity::ParseIntegrityAttribute(
        integrity_attribute, metadata_set, /*feature_context=*/nullptr);
    EXPECT_EQ(expected_metadata_set, metadata_set);
  }

  void ExpectParseFailure(const char* integrity_attribute) {
    IntegrityMetadataSet metadata_set;
    SubresourceIntegrity::ParseIntegrityAttribute(
        integrity_attribute, metadata_set, /*feature_context=*/nullptr);
    EXPECT_EQ(metadata_set.hashes.size(), 0u);
    EXPECT_EQ(metadata_set.public_keys.size(), 0u);
  }

  void ExpectEmptyParseResult(const char* integrity_attribute) {
    IntegrityMetadataSet metadata_set;

    SubresourceIntegrity::ParseIntegrityAttribute(
        integrity_attribute, metadata_set, /*feature_context=*/nullptr);
    EXPECT_EQ(0u, metadata_set.hashes.size());
    EXPECT_EQ(0u, metadata_set.public_keys.size());
  }

  enum ServiceWorkerMode {
    kNoServiceWorker,
    kSWOpaqueResponse,
    kSWClearResponse
  };

  enum Expectation { kIntegritySuccess, kIntegrityFailure };

  struct TestCase {
    const KURL url;
    network::mojom::RequestMode request_mode;
    network::mojom::FetchResponseType response_type;
    const Expectation expectation;
  };

  void CheckExpectedIntegrity(const char* integrity, const TestCase& test) {
    CheckExpectedIntegrity(integrity, test, test.expectation);
  }

  // Allows to overwrite the test expectation for cases that are always expected
  // to fail:
  void CheckExpectedIntegrity(const char* integrity,
                              const TestCase& test,
                              Expectation expectation) {
    IntegrityMetadataSet metadata_set;
    SubresourceIntegrity::ParseIntegrityAttribute(
        String(integrity), metadata_set, /*feature_context=*/nullptr);
    SegmentedBuffer buffer;
    buffer.Append(base::span_from_cstring(kBasicScript));
    IntegrityReport integrity_report;
    EXPECT_EQ(expectation == kIntegritySuccess,
              SubresourceIntegrity::CheckSubresourceIntegrity(
                  metadata_set, &buffer, test.url,
                  *CreateTestResource(test.url, test.request_mode,
                                      test.response_type),
                  nullptr, integrity_report, nullptr));
  }

  Resource* CreateTestResource(
      const KURL& url,
      network::mojom::RequestMode request_mode,
      network::mojom::FetchResponseType response_type) {
    ResourceRequest request;
    request.SetUrl(url);
    request.SetMode(request_mode);
    request.SetRequestorOrigin(SecurityOrigin::CreateUniqueOpaque());
    Resource* resource =
        RawResource::CreateForTest(request, ResourceType::kRaw);

    ResourceResponse response(url);
    response.SetHttpStatusCode(200);
    response.SetType(response_type);

    resource->SetResponse(response);
    return resource;
  }

  KURL sec_url;
  KURL insec_url;

  Persistent<MockFetchContext> context;
};

// Test the prioritization (i.e. selecting the "strongest" algorithm.
// This effectively tests the definition of IntegrityAlgorithm in
// IntegrityMetadata. The test is here, because SubresourceIntegrity is the
// class that relies on this working as expected.)
TEST_F(SubresourceIntegrityTest, Prioritization) {
  // Check that each algorithm is it's own "strongest".
  EXPECT_EQ(
      IntegrityAlgorithm::kSha256,
      std::max({IntegrityAlgorithm::kSha256, IntegrityAlgorithm::kSha256}));
  EXPECT_EQ(
      IntegrityAlgorithm::kSha384,
      std::max({IntegrityAlgorithm::kSha384, IntegrityAlgorithm::kSha384}));

  EXPECT_EQ(
      IntegrityAlgorithm::kSha512,
      std::max({IntegrityAlgorithm::kSha512, IntegrityAlgorithm::kSha512}));

  // Check a mix of algorithms.
  EXPECT_EQ(IntegrityAlgorithm::kSha384,
            std::max({IntegrityAlgorithm::kSha256, IntegrityAlgorithm::kSha384,
                      IntegrityAlgorithm::kSha256}));
  EXPECT_EQ(IntegrityAlgorithm::kSha512,
            std::max({IntegrityAlgorithm::kSha384, IntegrityAlgorithm::kSha512,
                      IntegrityAlgorithm::kSha256}));
}

TEST_F(SubresourceIntegrityTest, ParseAlgorithm) {
  ExpectAlgorithm("sha256-", IntegrityAlgorithm::kSha256);
  ExpectAlgorithm("sha384-", IntegrityAlgorithm::kSha384);
  ExpectAlgorithm("sha512-", IntegrityAlgorithm::kSha512);
  ExpectAlgorithm("sha-256-", IntegrityAlgorithm::kSha256);
  ExpectAlgorithm("sha-384-", IntegrityAlgorithm::kSha384);
  ExpectAlgorithm("sha-512-", IntegrityAlgorithm::kSha512);

  ExpectAlgorithmFailure("sha1-", SubresourceIntegrity::kAlgorithmUnknown);
  ExpectAlgorithmFailure("sha-1-", SubresourceIntegrity::kAlgorithmUnknown);
  ExpectAlgorithmFailure("foobarsha256-",
                         SubresourceIntegrity::kAlgorithmUnknown);
  ExpectAlgorithmFailure("foobar-", SubresourceIntegrity::kAlgorithmUnknown);
  ExpectAlgorithmFailure("-", SubresourceIntegrity::kAlgorithmUnknown);

  ExpectAlgorithmFailure("sha256", SubresourceIntegrity::kAlgorithmUnparsable);
  ExpectAlgorithmFailure("", SubresourceIntegrity::kAlgorithmUnparsable);
}

TEST_F(SubresourceIntegrityTest, ParseDigest) {
  ExpectDigest("abcdefg", "abcdefg");
  ExpectDigest("ab+de/g", "ab+de/g");
  ExpectDigest("ab-de_g", "ab+de/g");

  ExpectDigestFailure("abcdefg?");
  ExpectDigestFailure("?");
  ExpectDigestFailure("&&&foobar&&&");
  ExpectDigestFailure("\x01\x02\x03\x04");
}

//
// End-to-end parsing tests.
//

TEST_F(SubresourceIntegrityTest, Parsing) {
  ExpectParseFailure("not_really_a_valid_anything");
  ExpectParseFailure("sha256-&&&foobar&&&");
  ExpectParseFailure("sha256-\x01\x02\x03\x04");
  ExpectParseFailure("sha256-!!! sha256-!!!");

  ExpectEmptyParseResult("foobar:///sha256-abcdefg");
  ExpectEmptyParseResult("ni://sha256-abcdefg");
  ExpectEmptyParseResult("ni:///sha256-abcdefg");
  ExpectEmptyParseResult("notsha256atall-abcdefg");

  ExpectParse("sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
              "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
              IntegrityAlgorithm::kSha256);

  ExpectParse("sha-256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
              "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
              IntegrityAlgorithm::kSha256);

  ExpectParse("     sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=     ",
              "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
              IntegrityAlgorithm::kSha256);

  ExpectParse(
      "sha384-XVVXBGoYw6AJOh9J-Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup_tA1v5GPr",
      "XVVXBGoYw6AJOh9J+Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup/tA1v5GPr",
      IntegrityAlgorithm::kSha384);

  ExpectParse(
      "sha-384-XVVXBGoYw6AJOh9J_Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup_"
      "tA1v5GPr",
      "XVVXBGoYw6AJOh9J/Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup/tA1v5GPr",
      IntegrityAlgorithm::kSha384);

  ExpectParse(
      "sha512-tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ-"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
      "tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
      IntegrityAlgorithm::kSha512);

  ExpectParse(
      "sha-512-tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ-"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
      "tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
      IntegrityAlgorithm::kSha512);

  ExpectParse(
      "sha-512-tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ-"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==?ct=application/javascript",
      "tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
      IntegrityAlgorithm::kSha512);

  ExpectParse(
      "sha-512-tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ-"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==?ct=application/xhtml+xml",
      "tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
      IntegrityAlgorithm::kSha512);

  ExpectParse(
      "sha-512-tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ-"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==?foo=bar?ct=application/xhtml+xml",
      "tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
      IntegrityAlgorithm::kSha512);

  ExpectParse(
      "sha-512-tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ-"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==?ct=application/xhtml+xml?foo=bar",
      "tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
      IntegrityAlgorithm::kSha512);

  ExpectParse(
      "sha-512-tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ-"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==?baz=foz?ct=application/"
      "xhtml+xml?foo=bar",
      "tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
      IntegrityAlgorithm::kSha512);

  ExpectParseMultipleHashes("", {});
  ExpectParseMultipleHashes("    ", {});

  const IntegrityMetadata valid_sha384_and_sha512[] = {
      CreateIntegrityMetadata(
          "XVVXBGoYw6AJOh9J+Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup/tA1v5GPr",
          IntegrityAlgorithm::kSha384),
      CreateIntegrityMetadata(
          "tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+"
          "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
          IntegrityAlgorithm::kSha512),
  };
  ExpectParseMultipleHashes(
      "sha384-XVVXBGoYw6AJOh9J+Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup/tA1v5GPr "
      "sha512-tbUPioKbVBplr0b1ucnWB57SJWt4x9dOE0Vy2mzCXvH3FepqDZ+"
      "07yMK81ytlg0MPaIrPAjcHqba5csorDWtKg==",
      valid_sha384_and_sha512);

  const IntegrityMetadata valid_sha256_and_sha256[] = {
      CreateIntegrityMetadata("BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
                              IntegrityAlgorithm::kSha256),
      CreateIntegrityMetadata("deadbeef", IntegrityAlgorithm::kSha256),
  };
  ExpectParseMultipleHashes(
      "sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE= sha256-deadbeef",
      valid_sha256_and_sha256);

  const IntegrityMetadata valid_sha256_and_invalid_sha256[] = {
      CreateIntegrityMetadata("BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
                              IntegrityAlgorithm::kSha256),
  };
  ExpectParseMultipleHashes(
      "sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE= sha256-!!!!",
      valid_sha256_and_invalid_sha256);

  const IntegrityMetadata invalid_sha256_and_valid_sha256[] = {
      CreateIntegrityMetadata("BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
                              IntegrityAlgorithm::kSha256),
  };
  ExpectParseMultipleHashes(
      "sha256-!!! sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
      invalid_sha256_and_valid_sha256);

  ExpectParse("sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=?foo=bar",
              "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
              IntegrityAlgorithm::kSha256);

  ExpectParse(
      "sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=?foo=bar?baz=foz",
      "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
      IntegrityAlgorithm::kSha256);

  ExpectParse("sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=?",
              "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
              IntegrityAlgorithm::kSha256);
  ExpectParse("sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=?foo=bar",
              "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
              IntegrityAlgorithm::kSha256);
  ExpectParse(
      "sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=?foo=bar?baz=foz",
      "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
      IntegrityAlgorithm::kSha256);
  ExpectParse("sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=?foo",
              "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
              IntegrityAlgorithm::kSha256);
  ExpectParse("sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=?foo=bar?",
              "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
              IntegrityAlgorithm::kSha256);
  ExpectParse("sha256-BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=?foo:bar",
              "BpfBw7ivV8q2jLiT13fxDYAe2tJllusRSZ273h2nFSE=",
              IntegrityAlgorithm::kSha256);
}

TEST_F(SubresourceIntegrityTest, ParsingBase64) {
  ExpectParse(
      "sha384-XVVXBGoYw6AJOh9J+Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup/tA1v5GPr",
      "XVVXBGoYw6AJOh9J+Z8pBDMVVPfkBpngexkA7JqZu8d5GENND6TEIup/tA1v5GPr",
      IntegrityAlgorithm::kSha384);
}

// Tests that SubresourceIntegrity::CheckSubresourceIntegrity behaves correctly
// when faced with secure or insecure origins, same origin and cross origin
// requests, successful and failing CORS checks as well as when the response was
// handled by a service worker.
TEST_F(SubresourceIntegrityTest, OriginIntegrity) {
  constexpr auto kOk = kIntegritySuccess;
  constexpr auto kFail = kIntegrityFailure;
  const KURL& url = sec_url;

  const TestCase cases[] = {
      // FetchResponseType::kError never arrives because it is a loading error.
      {url, RequestMode::kNoCors, FetchResponseType::kBasic, kOk},
      {url, RequestMode::kNoCors, FetchResponseType::kCors, kOk},
      {url, RequestMode::kNoCors, FetchResponseType::kDefault, kOk},
      {url, RequestMode::kNoCors, FetchResponseType::kOpaque, kFail},
      {url, RequestMode::kNoCors, FetchResponseType::kOpaqueRedirect, kFail},

      // FetchResponseType::kError never arrives because it is a loading error.
      // FetchResponseType::kOpaque and FetchResponseType::kOpaqueResponse
      // never arrives: even when service worker is involved, it's handled as
      // an error.
      {url, RequestMode::kCors, FetchResponseType::kBasic, kOk},
      {url, RequestMode::kCors, FetchResponseType::kCors, kOk},
      {url, RequestMode::kCors, FetchResponseType::kDefault, kOk},
  };

  for (const auto& test : cases) {
    SCOPED_TRACE(testing::Message()
                 << ", target: " << test.url.BaseAsString()
                 << ", request mode: " << test.request_mode
                 << ", response type: " << test.response_type
                 << ", expected result: "
                 << (test.expectation == kIntegritySuccess ? "integrity"
                                                           : "failure"));

    // Verify basic sha256, sha384, and sha512 integrity checks.
    CheckExpectedIntegrity(kSha256Integrity, test);
    CheckExpectedIntegrity(kSha256IntegrityLenientSyntax, test);
    CheckExpectedIntegrity(kSha384Integrity, test);
    CheckExpectedIntegrity(kSha512Integrity, test);

    // Verify multiple hashes in an attribute.
    CheckExpectedIntegrity(kSha256AndSha384Integrities, test);
    CheckExpectedIntegrity(kBadSha256AndGoodSha384Integrities, test);

    // Unsupported hash functions should succeed.
    CheckExpectedIntegrity(kUnsupportedHashFunctionIntegrity, test);

    // Options should be ignored
    CheckExpectedIntegrity(kSha256IntegrityWithEmptyOption, test);
    CheckExpectedIntegrity(kSha256IntegrityWithOption, test);
    CheckExpectedIntegrity(kSha256IntegrityWithOptions, test);
    CheckExpectedIntegrity(kSha256IntegrityWithMimeOption, test);

    // The following tests are expected to fail in every scenario:

    // The hash label must match the hash value.
    CheckExpectedIntegrity(kSha384IntegrityLabeledAs256, test,
                           Expectation::kIntegrityFailure);

    // With multiple values, at least one must match, and it must be the
    // strongest hash algorithm.
    CheckExpectedIntegrity(kGoodSha256AndBadSha384Integrities, test,
                           Expectation::kIntegrityFailure);
    CheckExpectedIntegrity(kBadSha256AndBadSha384Integrities, test,
                           Expectation::kIntegrityFailure);
  }
}

// Test the prioritization of `IntegrityAlgorithm` enum values, as SRI's
// "strongest algorithm" selection mechanism depends upon the ordering
// below.
TEST_F(SubresourceIntegrityTest, AlgorithmEnumPrioritization) {
  // All the algorithms, listed in priority order: lowest to highest.
  IntegrityAlgorithm algs[] = {
      IntegrityAlgorithm::kSha256,
      IntegrityAlgorithm::kSha384,
      IntegrityAlgorithm::kSha512,
      IntegrityAlgorithm::kEd25519,
  };

  Vector<IntegrityAlgorithm> alg_vector;
  for (IntegrityAlgorithm alg : algs) {
    SCOPED_TRACE(alg);

    // Check that each algorithm is it's own "strongest".
    EXPECT_EQ(alg, std::max({alg, alg}));

    // Check that each algorithm in the text cases is stronger than the
    // previously-tested algorithms.
    alg_vector.push_back(alg);
    EXPECT_EQ(alg, *std::max_element(alg_vector.begin(), alg_vector.end()));
  }
}

TEST_F(SubresourceIntegrityTest, FindBestAlgorithm) {
  // All the algorithms, listed in priority order: lowest to highest.
  IntegrityAlgorithm algs[] = {
      IntegrityAlgorithm::kSha256,
      IntegrityAlgorithm::kSha384,
      IntegrityAlgorithm::kSha512,
      IntegrityAlgorithm::kEd25519,
  };

  Vector<IntegrityMetadata> alg_set;
  for (IntegrityAlgorithm alg : algs) {
    SCOPED_TRACE(alg);

    // Check that each algorithm is the strongest in a single-item list.
    EXPECT_EQ(alg, SubresourceIntegrity::FindBestAlgorithm(
                       {CreateIntegrityMetadata("", alg)}));

    // Check that each algorithm in the test cases is stronger than the
    // previously-tested algorithms.
    alg_set.push_back(CreateIntegrityMetadata("", alg));
    EXPECT_EQ(alg, SubresourceIntegrity::FindBestAlgorithm(alg_set));
  }
}

//
// Signature-based Tests
//
class SubresourceIntegritySignatureTest
    : public SubresourceIntegrityTest,
      public ::testing::WithParamInterface<bool> {
 public:
  class SignatureFeatureContext : public FeatureContext {
   public:
    explicit SignatureFeatureContext(bool status) : status_(status) {}

    // This would need to be more complicated if we had more than one feature
    // under test. Happily, we don't.
    bool FeatureEnabled(mojom::blink::OriginTrialFeature) const override {
      return status_;
    }
    RuntimeFeatureStateOverrideContext* GetRuntimeFeatureStateOverrideContext()
        const override {
      return nullptr;
    }

   private:
    bool status_;
  };

  SubresourceIntegritySignatureTest()
      : feature_context_(GetParam()),
        scoped_inline_integrity_(GetParam()) {}

  bool InlineSignaturesEnabled() { return GetParam(); }

  // Evaluates whether the given string is parsed into a single signature-based
  // IntegrityMetadata entry with the given digest.
  void ValidateSingleSignatureBasedItem(const String& integrity_attribute,
                                        const String& digest) {
    IntegrityMetadataSet metadata_set;
    SubresourceIntegrity::ParseIntegrityAttribute(
        integrity_attribute, metadata_set, /*feature_context=*/nullptr);
    EXPECT_EQ(0u, metadata_set.hashes.size());
      ASSERT_EQ(1u, metadata_set.public_keys.size());

      Vector<uint8_t> binary_digest;
      ASSERT_TRUE(Base64Decode(digest, binary_digest));

      network::IntegrityMetadata expected(IntegrityAlgorithm::kEd25519,
                                          std::move(binary_digest));
      EXPECT_EQ(expected, *metadata_set.public_keys.begin());
  }

  // Evalutes whether the given string is parsed into a set of IntegrityMetadata
  // items that contains each of the items in |hashes| and |public_keys|.
  void ValidateMultipleItems(const String& integrity_attribute,
                             const Vector<IntegrityMetadata>& hashes,
                             const Vector<IntegrityMetadata>& public_keys) {
    IntegrityMetadataSet metadata_set;
    SubresourceIntegrity::ParseIntegrityAttribute(
        integrity_attribute, metadata_set, /*feature_context=*/nullptr);

    // Validate hashes:
    ASSERT_EQ(hashes.size(), metadata_set.hashes.size());
    for (const auto& item : hashes) {
      EXPECT_TRUE(metadata_set.hashes.Contains(item));
    }

    // And then signatures:
      ASSERT_EQ(public_keys.size(), metadata_set.public_keys.size());
      for (const auto& item : public_keys) {
        EXPECT_TRUE(metadata_set.public_keys.Contains(item));
      }
  }

 protected:
  SignatureFeatureContext feature_context_;

 private:
  ScopedSignatureBasedInlineIntegrityForTest scoped_inline_integrity_;
};

INSTANTIATE_TEST_SUITE_P(FeatureFlag,
                         SubresourceIntegritySignatureTest,
                         ::testing::Bool());

TEST_P(SubresourceIntegritySignatureTest, ParseSignatureAlgorithm) {
  ExpectAlgorithm("ed25519-", IntegrityAlgorithm::kEd25519);
}

TEST_P(SubresourceIntegritySignatureTest, ParseSingleSignature) {
  String public_key_digest = "JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=";

  // Basic.
  ValidateSingleSignatureBasedItem("ed25519-" + public_key_digest,
                                   public_key_digest);
  // Leading space.
  ValidateSingleSignatureBasedItem("   ed25519-" + public_key_digest,
                                   public_key_digest);
  // Trailing space.
  ValidateSingleSignatureBasedItem("ed25519-" + public_key_digest + "   ",
                                   public_key_digest);
  // More space.
  ValidateSingleSignatureBasedItem("   ed25519-" + public_key_digest + "   ",
                                   public_key_digest);
  // Leading unknown.
  ValidateSingleSignatureBasedItem("unkno-wn   ed25519-" + public_key_digest,
                                   public_key_digest);
  // Trailing unknown.
  ValidateSingleSignatureBasedItem("ed25519-" + public_key_digest + " unkno-wn",
                                   public_key_digest);
  // More unknowns.
  ValidateSingleSignatureBasedItem(
      "unkno-wn ed25519-" + public_key_digest + " unkno-wn2",
      public_key_digest);
  // Leading invalid.
  ValidateSingleSignatureBasedItem("ed25519-!!! ed25519-" + public_key_digest,
                                   public_key_digest);
  // Trailing invalid.
  ValidateSingleSignatureBasedItem(
      "ed25519-" + public_key_digest + " ed25519-???", public_key_digest);
}

TEST_P(SubresourceIntegritySignatureTest, ParseMultipleSignatures) {
  Vector<IntegrityMetadata> signature_pairs = {
      CreateIntegrityMetadata("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
                              IntegrityAlgorithm::kEd25519),
      CreateIntegrityMetadata("BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB=",
                              IntegrityAlgorithm::kEd25519),
      CreateIntegrityMetadata("CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC=",
                              IntegrityAlgorithm::kEd25519),
  };

  do {
    StringBuilder attribute;
    for (const auto& pair : signature_pairs) {
      attribute.Append(AlgorithmToPrefix(pair.algorithm));
      attribute.Append(Base64Encode(pair.value));
      attribute.Append(' ');
    }
    SCOPED_TRACE(attribute.ToString());
    // Only valid items:
    ValidateMultipleItems(attribute.ToString(), {}, signature_pairs);

    // Valid + unknown:
    ValidateMultipleItems(attribute.ToString() + " unknown-alg", {},
                          signature_pairs);

    // Valid + invalid:
    ValidateMultipleItems(attribute.ToString() + " ed25519-???", {},
                          signature_pairs);
  } while (
      std::next_permutation(signature_pairs.begin(), signature_pairs.end()));
}

TEST_P(SubresourceIntegritySignatureTest, ParseBoth) {
  Vector<IntegrityMetadata> hash_pairs = {
      // "Hello, world."
      CreateIntegrityMetadata("+MO/YqmqPm/BYZwlDkir51GTc9Pt9BvmLrXcRRma8u8=",
                              IntegrityAlgorithm::kSha256),
      CreateIntegrityMetadata(
          "S7LmUoguRQsq3IHIZ0Xhm5jjCDqH6uUQbumuj5CnrIFDk+RyBW/dWuqzEiV4mPaB",
          IntegrityAlgorithm::kSha384),
      CreateIntegrityMetadata(
          "rQw3wx1psxXzqB8TyM3nAQlK2RcluhsNwxmcqXE2YbgoDW735o8TPmIR4"
          "uWpoxUERddvFwjgRSGw7gNPCwuvJg==",
          IntegrityAlgorithm::kSha512),
  };
  Vector<IntegrityMetadata> signature_pairs = {
      CreateIntegrityMetadata("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=",
                              IntegrityAlgorithm::kEd25519),
      CreateIntegrityMetadata("BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB=",
                              IntegrityAlgorithm::kEd25519),
      CreateIntegrityMetadata("CCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCCC=",
                              IntegrityAlgorithm::kEd25519),
  };

  do {
    StringBuilder attribute;
    for (const auto& pair : signature_pairs) {
      attribute.Append(AlgorithmToPrefix(pair.algorithm));
      attribute.Append(Base64Encode(pair.value));
      attribute.Append(' ');
    }
    for (const auto& pair : hash_pairs) {
      attribute.Append(AlgorithmToPrefix(pair.algorithm));
      attribute.Append(Base64Encode(pair.value));
      attribute.Append(' ');
    }
    SCOPED_TRACE(attribute.ToString());
    // Only valid items:
    ValidateMultipleItems(attribute.ToString(), hash_pairs, signature_pairs);

    // Valid + unknown:
    ValidateMultipleItems(attribute.ToString() + " unknown-alg", hash_pairs,
                          signature_pairs);

    // Valid + invalid:
    ValidateMultipleItems(attribute.ToString() + " ed25519-???", hash_pairs,
                          signature_pairs);
  } while (
      std::next_permutation(signature_pairs.begin(), signature_pairs.end()));
}

TEST_P(SubresourceIntegritySignatureTest, CheckEmpty) {
  // Regardless of the flag's state, no signatures + no requirements = valid.
  IntegrityReport integrity_report;
  IntegrityMetadataSet metadata_set;

  String raw_headers = "";
  EXPECT_TRUE(SubresourceIntegrity::CheckSubresourceIntegrity(
      metadata_set, /*buffer=*/nullptr, sec_url, FetchResponseType::kCors,
      raw_headers, /*feature_context=*/nullptr, integrity_report))
      << "Fetch variant";

  Resource* resource =
      CreateTestResource(sec_url, RequestMode::kCors, FetchResponseType::kCors);
  EXPECT_TRUE(SubresourceIntegrity::CheckSubresourceIntegrity(
      metadata_set, /*buffer=*/nullptr, sec_url, *resource,
      /*feature_context=*/nullptr, integrity_report, nullptr))
      << "Resource variant";
}

TEST_P(SubresourceIntegritySignatureTest, CheckNotSigned) {
  IntegrityReport integrity_report;
  IntegrityMetadataSet metadata_set;
  metadata_set.Insert(
      CreateIntegrityMetadata("", IntegrityAlgorithm::kEd25519));
  String raw_headers = "";

  // If the flag is set, the lack of a signature will fail any signature
  // integrity requirement.
  EXPECT_FALSE(SubresourceIntegrity::CheckSubresourceIntegrity(
      metadata_set, /*buffer=*/nullptr, sec_url, FetchResponseType::kCors,
      raw_headers, /*feature_context=*/nullptr, integrity_report));

  Resource* resource =
      CreateTestResource(sec_url, RequestMode::kCors, FetchResponseType::kCors);
  EXPECT_FALSE(SubresourceIntegrity::CheckSubresourceIntegrity(
      metadata_set, /*buffer=*/nullptr, sec_url, *resource,
      /*feature_context=*/nullptr, integrity_report, nullptr))
      << "Resource variant";
}

TEST_P(SubresourceIntegritySignatureTest, CheckValidSignature) {
  // Known-good message signature constants:
  String kPublicKey = "JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=";
  String kValidDigestHeader =
      "sha-256=:X48E9qOokqqrvdts8nOJRJN3OWDUoyWxBf7kbu9DBPE=:";
  String kValidSignatureInputHeader =
      "signature=(\"unencoded-digest\";sf);"
      "keyid=\"JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=\";tag=\"sri\"";
  String kValidSignatureHeader =
      "signature=:gHim9e5Pk2H7c9BStOmxSmkyc8+ioZgoxynu3d4INAT4dwfj5LhvaV9DFnEQ9"
      "p7C0hzW4o4Qpkm5aApd6WLLCw==:";

  String raw_headers =
      "HTTP/1.1 200 OK\r\n"
      "Unencoded-Digest: " +
      kValidDigestHeader +
      "\r\n"
      "Signature-Input: " +
      kValidSignatureInputHeader +
      "\r\n"
      "Signature: " +
      kValidSignatureHeader + "\r\n\r\n";

  IntegrityReport integrity_report;
  IntegrityMetadataSet metadata_set;
  metadata_set.public_keys = {
      CreateIntegrityMetadata(kPublicKey, IntegrityAlgorithm::kEd25519)};

  // Valid signature matching the integrity requirement should always pass.
  EXPECT_TRUE(SubresourceIntegrity::CheckSubresourceIntegrity(
      metadata_set, /*buffer=*/nullptr, sec_url, FetchResponseType::kCors,
      raw_headers, /*feature_context=*/nullptr, integrity_report));

  Resource* resource =
      CreateTestResource(sec_url, RequestMode::kCors, FetchResponseType::kCors);
  ResourceResponse& response = resource->GetMutableResponseForTesting();
  response.SetHttpHeaderField(http_names::kUnencodedDigest,
                              AtomicString(kValidDigestHeader));
  response.SetHttpHeaderField(http_names::kSignatureInput,
                              AtomicString(kValidSignatureInputHeader));
  response.SetHttpHeaderField(http_names::kSignature,
                              AtomicString(kValidSignatureHeader));
  EXPECT_TRUE(SubresourceIntegrity::CheckSubresourceIntegrity(
      metadata_set, /*buffer=*/nullptr, sec_url, *resource,
      /*feature_context=*/nullptr, integrity_report, nullptr))
      << "Resource variant";
}

TEST_P(SubresourceIntegritySignatureTest, Inline_NoSignatures) {
  String kIntegrity = "ed25519-JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=";
  String kSource = "alert(1);";

  const char* cases[] = {
      "",                            // Empty
      "   ",                         // Whitespace
      "invalid-signature",           // Invalid prefix
      "ed25519-invalid-base64ness",  // Invalid base64
      "ed25519-aaa",                 // Invalid length
  };

  for (const char* test : cases) {
    EXPECT_TRUE(SubresourceIntegrity::VerifyInlineIntegrity(
        kIntegrity, test, kSource, &feature_context_));
  }
}

TEST_P(SubresourceIntegritySignatureTest, Inline_MatchingSignature) {
  String kIntegrity = "ed25519-JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=";
  String kSource = "alert(1);";
  String kSignature =
      "ed25519-JXqOX/Ah0/d/"
      "QEVqirecUGwbPaVWGWXWF6CcZrhYlVwB2+"
      "64fDhfMKBXQna2RLH4lBBOPjWkZ8juVmIRItaoCQ==";

  EXPECT_TRUE(SubresourceIntegrity::VerifyInlineIntegrity(
      kIntegrity, kSignature, kSource, &feature_context_));
  // Valid + valid = Verified
  EXPECT_TRUE(SubresourceIntegrity::VerifyInlineIntegrity(
      kIntegrity, kSignature + " " + kSignature, kSource, &feature_context_));

  // Valid + invalid = Verified
  EXPECT_TRUE(SubresourceIntegrity::VerifyInlineIntegrity(
      kIntegrity, kSignature + " ed25519-aaa", kSource, &feature_context_));

  // Invalid + valid = Verified
  EXPECT_TRUE(SubresourceIntegrity::VerifyInlineIntegrity(
      kIntegrity, "ed25519-aaa " + kSignature, kSource, &feature_context_));
}

TEST_P(SubresourceIntegritySignatureTest, Inline_NonMatchingSignature) {
  String kIntegrity = "ed25519-JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=";
  String kSource = "alert(1);";
  String kSignature =
      "ed25519-"
      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
      "AAAAAAAAAAAAAA==";

  bool should_fail_verification = !InlineSignaturesEnabled();

  // Non-matching => Fail verification
  EXPECT_EQ(should_fail_verification,
            SubresourceIntegrity::VerifyInlineIntegrity(
                kIntegrity, kSignature, kSource, &feature_context_));

  // Non-matching + Non-matching => Fail verification
  EXPECT_EQ(should_fail_verification,
            SubresourceIntegrity::VerifyInlineIntegrity(
                kIntegrity, kSignature + " " + kSignature, kSource,
                &feature_context_));

  // Non-matching + invalid => Fail verification
  EXPECT_EQ(
      should_fail_verification,
      SubresourceIntegrity::VerifyInlineIntegrity(
          kIntegrity, kSignature + " ed25519-aaa", kSource, &feature_context_));

  // Invalid + non-matching => Fail verification
  EXPECT_EQ(
      should_fail_verification,
      SubresourceIntegrity::VerifyInlineIntegrity(
          kIntegrity, "ed25519-aaa " + kSignature, kSource, &feature_context_));
}

TEST_P(SubresourceIntegritySignatureTest, UseCounter) {
  // Just public key:
  {
    String kIntegrity = "ed25519-JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=";
    IntegrityMetadataSet metadata;
    IntegrityReport report;
    SubresourceIntegrity::ParseIntegrityAttribute(kIntegrity, metadata,
                                                  &feature_context_, &report);
    EXPECT_FALSE(
        report.UseCountersForTesting().Contains(WebFeature::kSRIHashAssertion));
    EXPECT_TRUE(report.UseCountersForTesting().Contains(
        WebFeature::kSRIPublicKeyAssertion));
  }
  // Just hash:
  {
    String kIntegrity = "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4=";
    IntegrityMetadataSet metadata;
    IntegrityReport report;
    SubresourceIntegrity::ParseIntegrityAttribute(kIntegrity, metadata,
                                                  &feature_context_, &report);
    EXPECT_TRUE(
        report.UseCountersForTesting().Contains(WebFeature::kSRIHashAssertion));
    EXPECT_FALSE(report.UseCountersForTesting().Contains(
        WebFeature::kSRIPublicKeyAssertion));
  }
  // Both:
  {
    String kIntegrity =
        "sha256-GAF48QOoxRvu0gZAmQivUdJPyBacqznBAXwnkfpmQX4= "
        "ed25519-JrQLj5P/89iXES9+vFgrIy29clF9CC/oPPsw3c5D0bs=";
    IntegrityMetadataSet metadata;
    IntegrityReport report;
    SubresourceIntegrity::ParseIntegrityAttribute(kIntegrity, metadata,
                                                  &feature_context_, &report);
    EXPECT_TRUE(
        report.UseCountersForTesting().Contains(WebFeature::kSRIHashAssertion));
    EXPECT_TRUE(report.UseCountersForTesting().Contains(
        WebFeature::kSRIPublicKeyAssertion));
  }
}

}  // namespace blink
