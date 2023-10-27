// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CERT_INTERNAL_TEST_HELPERS_H_
#define NET_CERT_INTERNAL_TEST_HELPERS_H_

#include <stddef.h>

#include <string>

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/boringssl/src/pki/parsed_certificate.h"

namespace net {

// Helper structure that maps a PEM block header (for instance "CERTIFICATE") to
// the destination where the value for that block should be written.
struct PemBlockMapping {
  // The name of the PEM header. Example "CERTIFICATE".
  const char* block_name;

  // The destination where the read value should be written to.
  std::string* value;

  // True to indicate that the block is not required to be present. If the
  // block is optional and is not present, then |value| will not be modified.
  bool optional = false;
};

// ReadTestDataFromPemFile() is a helper function that reads a PEM test file
// rooted in the "src/" directory.
//
//   * file_path_ascii:
//       The path to the PEM file, relative to src. For instance
//       "net/data/verify_signed_data_unittest/foopy.pem"
//
//   * mappings:
//       An array of length |mappings_length| which maps the expected PEM
//       headers to the destination to write its data.
//
// The function ensures that each of the chosen mappings is satisfied exactly
// once. In other words, the header must be present (unless marked as
// optional=true), have valid data, and appear no more than once.
::testing::AssertionResult ReadTestDataFromPemFile(
    const std::string& file_path_ascii,
    const PemBlockMapping* mappings,
    size_t mappings_length);

// This is the same as the variant above, however it uses template magic so an
// mappings array can be passed in directly (and the correct length is
// inferred).
template <size_t N>
::testing::AssertionResult ReadTestDataFromPemFile(
    const std::string& file_path_ascii,
    const PemBlockMapping (&mappings)[N]) {
  return ReadTestDataFromPemFile(file_path_ascii, mappings, N);
}

// Reads a certificate chain from |file_path_ascii|
bool ReadCertChainFromFile(const std::string& file_path_ascii,
                           bssl::ParsedCertificateList* chain);

// Reads a certificate from |file_path_ascii|. Returns nullptr if the file
// contained more that one certificate.
std::shared_ptr<const bssl::ParsedCertificate> ReadCertFromFile(
    const std::string& file_path_ascii);

// Reads a data file relative to the src root directory.
std::string ReadTestFileToString(const std::string& file_path_ascii);

}  // namespace net

#endif  // NET_CERT_INTERNAL_TEST_HELPERS_H_
