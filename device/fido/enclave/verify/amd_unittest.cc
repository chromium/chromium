// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/fido/enclave/verify/amd.h"

#include <iostream>

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/base.h"
#include "third_party/boringssl/src/include/openssl/bio.h"
#include "third_party/boringssl/src/include/openssl/buffer.h"
#include "third_party/boringssl/src/include/openssl/pem.h"
#include "third_party/boringssl/src/include/openssl/x509.h"

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

const base::FilePath::StringPieceType kArkMilanCertPath =
    FILE_PATH_LITERAL("device/fido/enclave/verify/testdata/ark_milan.pem");
const base::FilePath::StringPieceType kArkGenoaCertPath =
    FILE_PATH_LITERAL("device/fido/enclave/verify/testdata/ark_genoa.pem");
const base::FilePath::StringPieceType kAskMilanCertPath =
    FILE_PATH_LITERAL("device/fido/enclave/verify/testdata/ask_milan.pem");

TEST(AmdTest, VerifyCertSignature_WithValidSignature_Succeeds) {
  std::string ark = ReadContentsOfFile(kArkMilanCertPath);
  BIO *bp = BIO_new_mem_buf(ark.data(), ark.size());
  X509* x = nullptr;
  PEM_read_bio_X509(bp, &x, nullptr, nullptr);

  EXPECT_TRUE(VerifyCertSignature(x, x).has_value());

  BIO_free(bp);
  X509_free(x);
}

TEST(AmdTest, VerifyCertSignature_WithInvalidSignature_Fails) {
  std::string ark = ReadContentsOfFile(kArkMilanCertPath);
  std::string ask = ReadContentsOfFile(kAskMilanCertPath);
  BIO *bp_ark = BIO_new_mem_buf(ark.data(), ark.size());
  BIO *bp_ask = BIO_new_mem_buf(ask.data(), ask.size());
  X509* ark_cert = nullptr;
  X509* ask_cert = nullptr;
  PEM_read_bio_X509(bp_ark, &ark_cert, nullptr, nullptr);
  PEM_read_bio_X509(bp_ask, &ask_cert, nullptr, nullptr);

  EXPECT_FALSE(VerifyCertSignature(ask_cert, ark_cert).has_value());

  BIO_free(bp_ark);
  BIO_free(bp_ask);
  X509_free(ark_cert);
  X509_free(ask_cert);
}

TEST(AmdTest, ValidateArkAskCerts_WithValidCertsPair_Succeeds) {
  std::string ark = ReadContentsOfFile(kArkMilanCertPath);
  std::string ask = ReadContentsOfFile(kAskMilanCertPath);
  BIO *bp_ark = BIO_new_mem_buf(ark.data(), ark.size());
  BIO *bp_ask = BIO_new_mem_buf(ask.data(), ask.size());
  X509* ark_cert = nullptr;
  X509* ask_cert = nullptr;
  PEM_read_bio_X509(bp_ark, &ark_cert, nullptr, nullptr);
  PEM_read_bio_X509(bp_ask, &ask_cert, nullptr, nullptr);

  EXPECT_TRUE(ValidateArkAskCerts(ark_cert, ask_cert).has_value());

  BIO_free(bp_ark);
  BIO_free(bp_ask);
  X509_free(ark_cert);
  X509_free(ask_cert);
}

TEST(AmdTest, ValidateArkAskCerts_WithInvalidCertsPair_Fails) {
  std::string ark = ReadContentsOfFile(kArkGenoaCertPath);
  std::string ask = ReadContentsOfFile(kAskMilanCertPath);
  BIO *bp_ark = BIO_new_mem_buf(ark.data(), ark.size());
  BIO *bp_ask = BIO_new_mem_buf(ask.data(), ask.size());
  X509* ark_cert = nullptr;
  X509* ask_cert = nullptr;
  PEM_read_bio_X509(bp_ark, &ark_cert, nullptr, nullptr);
  PEM_read_bio_X509(bp_ask, &ask_cert, nullptr, nullptr);

  EXPECT_FALSE(ValidateArkAskCerts(ark_cert, ask_cert).has_value());

  BIO_free(bp_ark);
  BIO_free(bp_ask);
  X509_free(ark_cert);
  X509_free(ask_cert);
}

} // namespace

}  //  namespace device::enclave
