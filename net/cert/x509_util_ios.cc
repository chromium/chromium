// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_util_ios.h"

#include "net/cert/x509_certificate.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace net {

namespace x509_util {

base::ScopedCFTypeRef<SecCertificateRef> CreateSecCertificateFromBytes(
    const uint8_t* data,
    size_t length) {
  base::ScopedCFTypeRef<CFDataRef> cert_data(
      CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(data),
                   base::checked_cast<CFIndex>(length)));
  if (!cert_data)
    return base::ScopedCFTypeRef<SecCertificateRef>();

  return base::ScopedCFTypeRef<SecCertificateRef>(
      SecCertificateCreateWithData(nullptr, cert_data));
}

base::ScopedCFTypeRef<SecCertificateRef>
CreateSecCertificateFromX509Certificate(const X509Certificate* cert) {
  return CreateSecCertificateFromBytes(CRYPTO_BUFFER_data(cert->cert_buffer()),
                                       CRYPTO_BUFFER_len(cert->cert_buffer()));
}

scoped_refptr<X509Certificate> CreateX509CertificateFromSecCertificate(
    SecCertificateRef sec_cert,
    const std::vector<SecCertificateRef>& sec_chain) {
  if (!sec_cert)
    return nullptr;
  base::ScopedCFTypeRef<CFDataRef> der_data(SecCertificateCopyData(sec_cert));
  if (!der_data)
    return nullptr;
  bssl::UniquePtr<CRYPTO_BUFFER> cert_handle(
      X509Certificate::CreateCertBufferFromBytes(
          reinterpret_cast<const char*>(CFDataGetBytePtr(der_data)),
          CFDataGetLength(der_data)));
  if (!cert_handle)
    return nullptr;
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  for (const SecCertificateRef& sec_intermediate : sec_chain) {
    if (!sec_intermediate)
      return nullptr;
    der_data.reset(SecCertificateCopyData(sec_intermediate));
    if (!der_data)
      return nullptr;
    bssl::UniquePtr<CRYPTO_BUFFER> intermediate_cert_handle(
        X509Certificate::CreateCertBufferFromBytes(
            reinterpret_cast<const char*>(CFDataGetBytePtr(der_data)),
            CFDataGetLength(der_data)));
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
