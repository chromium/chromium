// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_util_apple.h"

#include <CommonCrypto/CommonDigest.h>

#include <string>

#include "base/check_op.h"
#include "base/logging.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "net/cert/x509_certificate.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace net {
namespace x509_util {

namespace {

bssl::UniquePtr<CRYPTO_BUFFER> CertBufferFromSecCertificate(
    SecCertificateRef sec_cert) {
  if (!sec_cert) {
    return nullptr;
  }
  base::ScopedCFTypeRef<CFDataRef> der_data(SecCertificateCopyData(sec_cert));
  if (!der_data) {
    return nullptr;
  }
  return X509Certificate::CreateCertBufferFromBytes(
      base::make_span(CFDataGetBytePtr(der_data),
                      base::checked_cast<size_t>(CFDataGetLength(der_data))));
}

}  // namespace

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
    base::ScopedCFTypeRef<SecCertificateRef> intermediate_cert(
        CreateSecCertificateFromBytes(CRYPTO_BUFFER_data(intermediate.get()),
                                      CRYPTO_BUFFER_len(intermediate.get())));
    if (!intermediate_cert) {
      if (invalid_intermediate_behavior == InvalidIntermediateBehavior::kFail)
        return base::ScopedCFTypeRef<CFMutableArrayRef>();
      LOG(WARNING) << "error parsing intermediate";
      continue;
    }
    CFArrayAppendValue(cert_list, intermediate_cert);
  }
  return cert_list;
}

scoped_refptr<X509Certificate> CreateX509CertificateFromSecCertificate(
    base::ScopedCFTypeRef<SecCertificateRef> sec_cert,
    const std::vector<base::ScopedCFTypeRef<SecCertificateRef>>& sec_chain) {
  return CreateX509CertificateFromSecCertificate(sec_cert, sec_chain, {});
}

scoped_refptr<X509Certificate> CreateX509CertificateFromSecCertificate(
    base::ScopedCFTypeRef<SecCertificateRef> sec_cert,
    const std::vector<base::ScopedCFTypeRef<SecCertificateRef>>& sec_chain,
    X509Certificate::UnsafeCreateOptions options) {
  bssl::UniquePtr<CRYPTO_BUFFER> cert_handle =
      CertBufferFromSecCertificate(sec_cert);
  if (!cert_handle) {
    return nullptr;
  }
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  for (const auto& sec_intermediate : sec_chain) {
    bssl::UniquePtr<CRYPTO_BUFFER> intermediate_cert_handle =
        CertBufferFromSecCertificate(sec_intermediate);
    if (!intermediate_cert_handle) {
      return nullptr;
    }
    intermediates.push_back(std::move(intermediate_cert_handle));
  }
  scoped_refptr<X509Certificate> result(
      X509Certificate::CreateFromBufferUnsafeOptions(
          std::move(cert_handle), std::move(intermediates), options));
  return result;
}

SHA256HashValue CalculateFingerprint256(SecCertificateRef cert) {
  SHA256HashValue sha256;
  memset(sha256.data, 0, sizeof(sha256.data));

  base::ScopedCFTypeRef<CFDataRef> cert_data(SecCertificateCopyData(cert));
  if (!cert_data) {
    return sha256;
  }

  DCHECK(CFDataGetBytePtr(cert_data));
  DCHECK_NE(CFDataGetLength(cert_data), 0);

  CC_SHA256(CFDataGetBytePtr(cert_data), CFDataGetLength(cert_data),
            sha256.data);

  return sha256;
}

base::ScopedCFTypeRef<CFArrayRef> CertificateChainFromSecTrust(
    SecTrustRef trust) {
  if (__builtin_available(macOS 12.0, iOS 15.0, *)) {
    return base::ScopedCFTypeRef<CFArrayRef>(
        SecTrustCopyCertificateChain(trust));
  }

  base::ScopedCFTypeRef<CFMutableArrayRef> chain(
      CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));
  const CFIndex chain_length = SecTrustGetCertificateCount(trust);
  for (CFIndex i = 0; i < chain_length; ++i) {
    CFArrayAppendValue(chain, SecTrustGetCertificateAtIndex(trust, i));
  }
  return base::ScopedCFTypeRef<CFArrayRef>(chain.release());
}

}  // namespace x509_util
}  // namespace net
