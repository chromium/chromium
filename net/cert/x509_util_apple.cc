// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/cert/x509_util_apple.h"

#include <CommonCrypto/CommonDigest.h>

#include <string>

#include "base/apple/foundation_util.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
#include "crypto/hash.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/include/openssl/pool.h"

namespace net {
namespace x509_util {

namespace {

bssl::UniquePtr<CRYPTO_BUFFER> CertBufferFromSecCertificate(
    SecCertificateRef sec_cert) {
  if (!sec_cert) {
    return nullptr;
  }
  base::apple::ScopedCFTypeRef<CFDataRef> der_data(
      SecCertificateCopyData(sec_cert));
  if (!der_data) {
    return nullptr;
  }
  return CreateCryptoBuffer(base::apple::CFDataToSpan(der_data.get()));
}

}  // namespace

base::apple::ScopedCFTypeRef<SecCertificateRef> CreateSecCertificateFromBytes(
    base::span<const uint8_t> data) {
  base::apple::ScopedCFTypeRef<CFDataRef> cert_data(CFDataCreate(
      kCFAllocatorDefault, reinterpret_cast<const UInt8*>(data.data()),
      base::checked_cast<CFIndex>(data.size())));
  if (!cert_data) {
    return base::apple::ScopedCFTypeRef<SecCertificateRef>();
  }

  return base::apple::ScopedCFTypeRef<SecCertificateRef>(
      SecCertificateCreateWithData(nullptr, cert_data.get()));
}

base::apple::ScopedCFTypeRef<SecCertificateRef>
CreateSecCertificateFromX509Certificate(const X509Certificate* cert) {
  return CreateSecCertificateFromBytes(CryptoBufferAsSpan(cert->cert_buffer()));
}

base::apple::ScopedCFTypeRef<CFMutableArrayRef>
CreateSecCertificateArrayForX509Certificate(X509Certificate* cert) {
  return CreateSecCertificateArrayForX509Certificate(
      cert, InvalidIntermediateBehavior::kFail);
}

base::apple::ScopedCFTypeRef<CFMutableArrayRef>
CreateSecCertificateArrayForX509Certificate(
    X509Certificate* cert,
    InvalidIntermediateBehavior invalid_intermediate_behavior) {
  base::apple::ScopedCFTypeRef<CFMutableArrayRef> cert_list(
      CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));
  if (!cert_list)
    return base::apple::ScopedCFTypeRef<CFMutableArrayRef>();
  std::string bytes;
  base::apple::ScopedCFTypeRef<SecCertificateRef> sec_cert(
      CreateSecCertificateFromBytes(CryptoBufferAsSpan(cert->cert_buffer())));
  if (!sec_cert) {
    return base::apple::ScopedCFTypeRef<CFMutableArrayRef>();
  }
  CFArrayAppendValue(cert_list.get(), sec_cert.get());
  for (const auto& intermediate : cert->intermediate_buffers()) {
    base::apple::ScopedCFTypeRef<SecCertificateRef> intermediate_cert(
        CreateSecCertificateFromBytes(CryptoBufferAsSpan(intermediate.get())));
    if (!intermediate_cert) {
      if (invalid_intermediate_behavior == InvalidIntermediateBehavior::kFail)
        return base::apple::ScopedCFTypeRef<CFMutableArrayRef>();
      LOG(WARNING) << "error parsing intermediate";
      continue;
    }
    CFArrayAppendValue(cert_list.get(), intermediate_cert.get());
  }
  return cert_list;
}

scoped_refptr<X509Certificate> CreateX509CertificateFromSecCertificate(
    base::apple::ScopedCFTypeRef<SecCertificateRef> sec_cert,
    const std::vector<base::apple::ScopedCFTypeRef<SecCertificateRef>>&
        sec_chain) {
  return CreateX509CertificateFromSecCertificate(sec_cert, sec_chain, {});
}

scoped_refptr<X509Certificate> CreateX509CertificateFromSecCertificate(
    base::apple::ScopedCFTypeRef<SecCertificateRef> sec_cert,
    const std::vector<base::apple::ScopedCFTypeRef<SecCertificateRef>>&
        sec_chain,
    X509Certificate::UnsafeCreateOptions options) {
  bssl::UniquePtr<CRYPTO_BUFFER> cert_handle =
      CertBufferFromSecCertificate(sec_cert.get());
  if (!cert_handle) {
    return nullptr;
  }
  std::vector<bssl::UniquePtr<CRYPTO_BUFFER>> intermediates;
  for (const auto& sec_intermediate : sec_chain) {
    bssl::UniquePtr<CRYPTO_BUFFER> intermediate_cert_handle =
        CertBufferFromSecCertificate(sec_intermediate.get());
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
  SHA256HashValue sha256 = {0};

  base::apple::ScopedCFTypeRef<CFDataRef> cert_data(
      SecCertificateCopyData(cert));
  if (!cert_data) {
    return sha256;
  }

  DCHECK(CFDataGetBytePtr(cert_data.get()));
  DCHECK_NE(CFDataGetLength(cert_data.get()), 0);

  return crypto::hash::Sha256(base::apple::CFDataToSpan(cert_data.get()));
}

}  // namespace x509_util
}  // namespace net
