// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_certificate.h"
#include "third_party/fuzztest/src/fuzztest/fuzztest.h"

namespace net {

namespace {

void FuzzCreateFromDERCertChain(
    const std::vector<std::string_view>& der_certs) {
  X509Certificate::CreateFromDERCertChain(der_certs);
}

FUZZ_TEST(X509CertificateFuzzTest, FuzzCreateFromDERCertChain);

}  // namespace

}  // namespace net
