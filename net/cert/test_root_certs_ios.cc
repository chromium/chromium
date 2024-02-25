// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/test_root_certs.h"

#include <Security/Security.h>

#include "build/build_config.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/cert/x509_util_apple.h"

namespace net {

bool TestRootCerts::AddImpl(X509Certificate* certificate) {
  base::apple::ScopedCFTypeRef<SecCertificateRef> os_cert(
      x509_util::CreateSecCertificateFromX509Certificate(certificate));
  if (!os_cert) {
    return false;
  }

  if (CFArrayContainsValue(
          temporary_roots_.get(),
          CFRangeMake(0, CFArrayGetCount(temporary_roots_.get())),
          os_cert.get())) {
    return true;
  }
  CFArrayAppendValue(temporary_roots_.get(), os_cert.get());

  return true;
}

void TestRootCerts::ClearImpl() {
  CFArrayRemoveAllValues(temporary_roots_.get());
}

OSStatus TestRootCerts::FixupSecTrustRef(SecTrustRef trust_ref) const {
  if (IsEmpty()) {
    return noErr;
  }

  OSStatus status =
      SecTrustSetAnchorCertificates(trust_ref, temporary_roots_.get());
  if (status) {
    return status;
  }
  // Trust system store in addition to trusting |temporary_roots_|.
  return SecTrustSetAnchorCertificatesOnly(trust_ref, false);
}

TestRootCerts::~TestRootCerts() = default;

void TestRootCerts::Init() {
  temporary_roots_.reset(
      CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));
}

}  // namespace net
