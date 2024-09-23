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
  auto test_raw_span = base::make_span(
      static_cast<const uint8_t*>((uint8_t*)test_raw.data()), test_raw.size());
  auto res_span = base::make_span(
      static_cast<const uint8_t*>((uint8_t*)res->data()), res->size());
  EXPECT_TRUE(device::enclave::EqualKeys(test_raw_span, res_span).value());
}

TEST(UtilsTest, ConvertPemToRaw_WithInvalidPem_ReturnsError) {
  auto res = device::enclave::ConvertPemToRaw("Not a valid PEM");

  EXPECT_FALSE(res.has_value());
}

TEST(UtilsTest, ConvertRawToPem_ReturnsPem) {
  auto test_pem = ReadContentsOfFile(kTestPemPath);
  auto test_raw = ReadContentsOfFile(kTestRawPath);
  base::span<const uint8_t> temp = base::make_span(
      static_cast<const uint8_t*>((uint8_t*)test_raw.data()), test_raw.size());

  auto res = device::enclave::ConvertRawToPem(temp);

  EXPECT_EQ(res, test_pem);
}

TEST(UtilsTest, VerifySignatureRaw_WithValidSignature_Succeeds) {
  auto test_digest = ReadContentsOfFile(kTestDigestPath);
  auto test_digest_signature = ReadContentsOfFile(kTestSignaturePath);
  auto test_raw = ConvertPemToRaw(ReadContentsOfFile(kTestRekorPath));
  EXPECT_TRUE(test_raw.has_value());
  base::span<const uint8_t> test_digest_span =
      base::make_span(static_cast<const uint8_t*>((uint8_t*)test_digest.data()),
                      test_digest.size());
  base::span<const uint8_t> test_digest_sig_span = base::make_span(
      static_cast<const uint8_t*>((uint8_t*)test_digest_signature.data()),
      test_digest_signature.size());
  base::span<const uint8_t> test_raw_span =
      base::make_span(static_cast<const uint8_t*>((uint8_t*)test_raw->data()),
                      test_raw->size());

  auto res = device::enclave::VerifySignatureRaw(
      test_digest_sig_span, test_digest_span, test_raw_span);

  EXPECT_TRUE(res.has_value());
}

TEST(UtilsTest, VerifySignatureRaw_WithInvalidSignature_Fails) {
  auto test_digest = ReadContentsOfFile(kTestDigestPath);
  auto test_raw = ConvertPemToRaw(ReadContentsOfFile(kTestRekorPath));
  EXPECT_TRUE(test_raw.has_value());
  base::span<const uint8_t> test_digest_span =
      base::make_span(static_cast<const uint8_t*>((uint8_t*)test_digest.data()),
                      test_digest.size());
  base::span<const uint8_t> test_raw_span =
      base::make_span(static_cast<const uint8_t*>((uint8_t*)test_raw->data()),
                      test_raw->size());

  auto res = device::enclave::VerifySignatureRaw(
      kInvalidSignature, test_digest_span, test_raw_span);

  EXPECT_FALSE(res.has_value());
}

TEST(UtilsTest, EqualKeys_WithEqualKeys_ReturnsTrue) {
  auto test_raw = ReadContentsOfFile(kTestRawPath);
  auto test_raw_span = base::make_span(
      static_cast<const uint8_t*>((uint8_t*)test_raw.data()), test_raw.size());

  EXPECT_TRUE(device::enclave::EqualKeys(test_raw_span, test_raw_span).value());
}

TEST(UtilsTest, EqualKeys_WithUnequalKeys_ReturnsFalse) {
  auto test_raw = ReadContentsOfFile(kTestRawPath);
  auto test_alternate_raw = ReadContentsOfFile(kTestAlternateRawPath);
  auto test_raw_span = base::make_span(
      static_cast<const uint8_t*>((uint8_t*)test_raw.data()), test_raw.size());
  auto test_alternate_raw_span = base::make_span(
      static_cast<const uint8_t*>((uint8_t*)test_alternate_raw.data()),
      test_alternate_raw.size());
  EXPECT_FALSE(
      device::enclave::EqualKeys(test_raw_span, test_alternate_raw_span)
          .value());
}

} // namespace

}  // namespace device::enclave
