// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_util_ios_and_mac.h"

#include "base/logging.h"
#include "net/cert/x509_certificate.h"
#if defined(OS_IOS)
#include "net/cert/x509_util_ios.h"
#else
#include "net/cert/x509_util_mac.h"
#endif
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace net {

namespace x509_util {

base::ScopedCFTypeRef<CFMutableArrayRef>
CreateSecCertificateArrayForX509Certificate(X509Certificate* cert) {
  return CreateSecCertificateArrayForX509Certificate(
      cert, InvalidIntermediateBehavior::kFail);
}

base::ScopedCFTypeRef<CFMutableArrayRef>
CreateSecCertificateArrayForX509Certificate(
    X509Certificate* cert,
    InvalidIntermediateBehavior invalid_intermediate_behavior) {
  base::ScopedCFTypeRef<CFMutableArrayRef> cert_list(
      CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));
  if (!cert_list)
    return base::ScopedCFTypeRef<CFMutableArrayRef>();
  std::string bytes;
  base::ScopedCFTypeRef<SecCertificateRef> sec_cert(
      CreateSecCertificateFromBytes(CRYPTO_BUFFER_data(cert->cert_buffer()),
                                    CRYPTO_BUFFER_len(cert->cert_buffer())));
  if (!sec_cert)
    return base::ScopedCFTypeRef<CFMutableArrayRef>();
  CFArrayAppendValue(cert_list, sec_cert);
  for (const auto& intermediate : cert->intermediate_buffers()) {
    base::ScopedCFTypeRef<SecCertificateRef> sec_cert(
        CreateSecCertificateFromBytes(CRYPTO_BUFFER_data(intermediate.get()),
                                      CRYPTO_BUFFER_len(intermediate.get())));
    if (!sec_cert) {
      if (invalid_intermediate_behavior == InvalidIntermediateBehavior::kFail)
        return base::ScopedCFTypeRef<CFMutableArrayRef>();
      LOG(WARNING) << "error parsing intermediate";
      continue;
    }
    CFArrayAppendValue(cert_list, sec_cert);
  }
  return cert_list;
}

}  // namespace x509_util

}  // namespace net
