// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "device/fido/enclave/verify/utils.h"

#include "base/base_paths.h"
#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device::enclave {

namespace {

std::string ReadContentsOfFile(
    base::FilePath::StringPieceType file_path_string) {
  base::FilePath file_path;
  base::PathService::Get(base::BasePathKey::DIR_SRC_TEST_DATA_ROOT, &file_path);
  file_path = file_path.Append(file_path_string);
  std::string result;
  EXPECT_TRUE(base::ReadFileToString(file_path, &result));
  return result;
}

const base::FilePath::StringPieceType kTestDigestPath =
    FILE_PATH_LITERAL("device/fido/enclave/verify/testdata/test_digest.txt");
const base::FilePath::StringPieceType kTestSignaturePath =
    FILE_PATH_LITERAL("device/fido/enclave/verify/testdata/test_signature");
const base::FilePath::StringPieceType kTestPemPath =
    FILE_PATH_LITERAL("device/fido/enclave/verify/testdata/test_pub_key.pem");
const base::FilePath::StringPieceType kTestRawPath =
    FILE_PATH_LITERAL("device/fido/enclave/verify/testdata/test_pub_key.der");
const base::FilePath::StringPieceType kTestRekorPath =
    FILE_PATH_LITERAL("device/fido/enclave/verify/testdata/rekor_pub_key.pem");
const base::FilePath::StringPieceType kTestAlternateRawPath = FILE_PATH_LITERAL(
    "device/fido/enclave/verify/testdata/test_alternate_pub_key.der");
const uint8_t kInvalidSignature[] = {1, 2, 3, 4};

TEST(UtilsTest, LooksLikePem_WithValidPem_ReturnsTrue) {
  auto test_pem = ReadContentsOfFile(kTestPemPath);

  EXPECT_TRUE(device::enclave::LooksLikePem(test_pem));
}

TEST(UtilsTest, LooksLikePem_WithInvalidPem_ReturnsFalse) {
  EXPECT_FALSE(device::enclave::LooksLikePem("This should return false"));
}

TEST(UtilsTest, ConvertPemToRaw_WithValidPem_ReturnsRaw) {
  auto test_pem = ReadContentsOfFile(kTestPemPath);
  auto test_raw = ReadContentsOfFile(kTestRawPath);

  auto res = device::enclave::ConvertPemToRaw(test_pem);

  EXPECT_TRUE(res.has_value());
  EXPECT_EQ(std::string(res->begin(), res->end()), test_raw);
  EXPECT_TRUE(device::enclave::EqualKeys(base::as_byte_span(test_raw),
                                         base::as_byte_span(*res))
                  .value());
}

TEST(UtilsTest, ConvertPemToRaw_WithInvalidPem_ReturnsError) {
  auto res = device::enclave::ConvertPemToRaw("Not a valid PEM");

  EXPECT_FALSE(res.has_value());
}

TEST(UtilsTest, ConvertRawToPem_ReturnsPem) {
  auto test_pem = ReadContentsOfFile(kTestPemPath);
  auto test_raw = ReadContentsOfFile(kTestRawPath);

  auto res = device::enclave::ConvertRawToPem(base::as_byte_span(test_raw));

  EXPECT_EQ(res, test_pem);
}

TEST(UtilsTest, VerifySignatureRaw_WithValidSignature_Succeeds) {
  auto test_digest = ReadContentsOfFile(kTestDigestPath);
  auto test_digest_signature = ReadContentsOfFile(kTestSignaturePath);
  auto test_raw = ConvertPemToRaw(ReadContentsOfFile(kTestRekorPath));
  EXPECT_TRUE(test_raw.has_value());

  auto res = device::enclave::VerifySignatureRaw(
      base::as_byte_span(test_digest_signature),
      base::as_byte_span(test_digest), base::as_byte_span(*test_raw));

  EXPECT_TRUE(res.has_value());
}

TEST(UtilsTest, VerifySignatureRaw_WithInvalidSignature_Fails) {
  auto test_digest = ReadContentsOfFile(kTestDigestPath);
  auto test_raw = ConvertPemToRaw(ReadContentsOfFile(kTestRekorPath));
  EXPECT_TRUE(test_raw.has_value());

  auto res = device::enclave::VerifySignatureRaw(
      kInvalidSignature, base::as_byte_span(test_digest),
      base::as_byte_span(*test_raw));

  EXPECT_FALSE(res.has_value());
}

TEST(UtilsTest, EqualKeys_WithEqualKeys_ReturnsTrue) {
  auto test_raw = ReadContentsOfFile(kTestRawPath);

  EXPECT_TRUE(device::enclave::EqualKeys(base::as_byte_span(test_raw),
                                         base::as_byte_span(test_raw))
                  .value());
}

TEST(UtilsTest, EqualKeys_WithUnequalKeys_ReturnsFalse) {
  auto test_raw = ReadContentsOfFile(kTestRawPath);
  auto test_alternate_raw = ReadContentsOfFile(kTestAlternateRawPath);
  EXPECT_FALSE(
      device::enclave::EqualKeys(base::as_byte_span(test_raw),
                                 base::as_byte_span(test_alternate_raw))
          .value());
}

} // namespace

}  // namespace device::enclave
