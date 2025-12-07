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
