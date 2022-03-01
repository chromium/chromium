// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_util_ios.h"

#include "net/cert/x509_certificate.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace net {

namespace x509_util {

scoped_refptr<X509Certificate> CreateX509CertificateFromSecCertificate(
    base::ScopedCFTypeRef<SecCertificateRef> sec_cert,
    const std::vector<base::ScopedCFTypeRef<SecCertificateRef>>& sec_chain) {
  if (!sec_cert)
    return nullptr;
  base::ScopedCFTypeRef<CFDataRef> der_data(SecCertificateCopyData(sec_cert));
  if (!der_data)
    return nullptr;
  bssl::UniquePtr<CRYPTO_BUFFER> cert_handle(
      X509Certificate::CreateCertBufferFromBytes(base::make_span(
          CFDataGetBytePtr(der_data), CFDataGetLength(der_data))));
  if (!cert_handle)
    return nullptr;
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  for (const auto& sec_intermediate : sec_chain) {
    if (!sec_intermediate.get())
      return nullptr;
    der_data.reset(SecCertificateCopyData(sec_intermediate));
    if (!der_data)
      return nullptr;
    bssl::UniquePtr<CRYPTO_BUFFER> intermediate_cert_handle(
        X509Certificate::CreateCertBufferFromBytes(base::make_span(
            CFDataGetBytePtr(der_data), CFDataGetLength(der_data))));
    if (!intermediate_cert_handle)
      return nullptr;
    intermediates.push_back(std::move(intermediate_cert_handle));
  }
  scoped_refptr<X509Certificate> result(X509Certificate::CreateFromBuffer(
      std::move(cert_handle), std::move(intermediates)));
  return result;
}

}  // namespace x509_util

}  // namespace net
