// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/test_root_certs.h"

#include "base/location.h"
#include "net/android/network_library.h"
#include "net/cert/x509_certificate.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace net {

bool TestRootCerts::AddImpl(X509Certificate* certificate) {
  android::AddTestRootCertificate(certificate->cert_span());
  return true;
}

void TestRootCerts::ClearImpl() {
  if (IsEmpty())
    return;

  android::ClearTestRootCertificates();
}

TestRootCerts::~TestRootCerts() = default;

void TestRootCerts::Init() {}

}  // namespace net
