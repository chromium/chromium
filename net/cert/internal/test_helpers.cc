// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/cert/internal/test_helpers.h"

#include "base/base_paths.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/pki/cert_errors.h"
#include "third_party/boringssl/src/pki/pem.h"

namespace net {

::testing::AssertionResult ReadTestDataFromPemFile(
    const std::string& file_path_ascii,
    const PemBlockMapping* mappings,
    size_t mappings_length) {
  std::string file_data = ReadTestFileToString(file_path_ascii);

  // mappings_copy is used to keep track of which mappings have already been
  // satisfied (by nulling the |value| field). This is used to track when
  // blocks are multiply defined.
  std::vector<PemBlockMapping> mappings_copy(mappings,
                                             mappings + mappings_length);

  // Build the |pem_headers| vector needed for PEMTokenzier.
  std::vector<std::string> pem_headers;
  for (const auto& mapping : mappings_copy) {
    pem_headers.push_back(mapping.block_name);
  }

  bssl::PEMTokenizer pem_tokenizer(file_data, pem_headers);
  while (pem_tokenizer.GetNext()) {
    for (auto& mapping : mappings_copy) {
      // Find the mapping for this block type.
      if (pem_tokenizer.block_type() == mapping.block_name) {
        if (!mapping.value) {
          return ::testing::AssertionFailure()
                 << "PEM block defined multiple times: " << mapping.block_name;
        }

        // Copy the data to the result.
        mapping.value->assign(pem_tokenizer.data());

        // Mark the mapping as having been satisfied.
        mapping.value = nullptr;
      }
    }
  }

  // Ensure that all specified blocks were found.
  for (const auto& mapping : mappings_copy) {
    if (mapping.value && !mapping.optional) {
      return ::testing::AssertionFailure()
             << "PEM block missing: " << mapping.block_name;
    }
  }

  return ::testing::AssertionSuccess();
}

bool ReadCertChainFromFile(const std::string& file_path_ascii,
                           bssl::ParsedCertificateList* chain) {
  // Reset all the out parameters to their defaults.
  chain->clear();

  std::string file_data = ReadTestFileToString(file_path_ascii);
  if (file_data.empty()) {
    return false;
  }

  std::vector<std::string> pem_headers = {"CERTIFICATE"};

  bssl::PEMTokenizer pem_tokenizer(file_data, pem_headers);
  while (pem_tokenizer.GetNext()) {
    const std::string& block_data = pem_tokenizer.data();

    bssl::CertErrors errors;
    if (!bssl::ParsedCertificate::CreateAndAddToVector(
            bssl::UniquePtr<CRYPTO_BUFFER>(CRYPTO_BUFFER_new(
                reinterpret_cast<const uint8_t*>(block_data.data()),
                block_data.size(), nullptr)),
            {}, chain, &errors)) {
      ADD_FAILURE() << errors.ToDebugString();
      return false;
    }
  }

  return true;
}

std::shared_ptr<const bssl::ParsedCertificate> ReadCertFromFile(
    const std::string& file_path_ascii) {
  bssl::ParsedCertificateList chain;
  if (!ReadCertChainFromFile(file_path_ascii, &chain)) {
    return nullptr;
  }
  if (chain.size() != 1) {
    return nullptr;
  }
  return chain[0];
}

std::string ReadTestFileToString(const std::string& file_path_ascii) {
  // Compute the full path, relative to the src/ directory.
  base::FilePath src_root;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &src_root);
  base::FilePath filepath = src_root.AppendASCII(file_path_ascii);

  // Read the full contents of the file.
  std::string file_data;
  if (!base::ReadFileToString(filepath, &file_data)) {
    ADD_FAILURE() << "Couldn't read file: " << filepath.value();
    return std::string();
  }

  return file_data;
}

}  // namespace net
