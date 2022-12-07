// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "net/cert/pki/cert_errors.h"
#include "net/cert/pki/parsed_certificate.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/include/openssl/span.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  net::CertErrors errors;
  std::shared_ptr<const net::ParsedCertificate> cert =
      net::ParsedCertificate::Create(
          net::x509_util::CreateCryptoBuffer(base::make_span(data, size)), {},
          &errors);

  // Severe errors must be provided iff the parsing failed.
  CHECK_EQ(errors.ContainsAnyErrorWithSeverity(net::CertError::SEVERITY_HIGH),
           cert == nullptr);

  return 0;
}
