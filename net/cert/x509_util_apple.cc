// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "net/cert/x509_util_apple.h"

#include <CommonCrypto/CommonDigest.h>

#include <string>

#include "base/apple/foundation_util.h"
#include "base/check_op.h"
#include "base/logging.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "build/build_config.h"
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
  SHA256HashValue sha256;
  memset(sha256.data, 0, sizeof(sha256.data));

  base::apple::ScopedCFTypeRef<CFDataRef> cert_data(
      SecCertificateCopyData(cert));
  if (!cert_data) {
    return sha256;
  }

  DCHECK(CFDataGetBytePtr(cert_data.get()));
  DCHECK_NE(CFDataGetLength(cert_data.get()), 0);

  CC_SHA256(CFDataGetBytePtr(cert_data.get()), CFDataGetLength(cert_data.get()),
            sha256.data);

  return sha256;
}

base::apple::ScopedCFTypeRef<CFArrayRef> CertificateChainFromSecTrust(
    SecTrustRef trust) {
  if (__builtin_available(macOS 12.0, iOS 15.0, *)) {
    return base::apple::ScopedCFTypeRef<CFArrayRef>(
        SecTrustCopyCertificateChain(trust));
  }

// TODO(crbug.com/40899365): Remove code when it is no longer needed.
#if (BUILDFLAG(IS_MAC) &&                                    \
     MAC_OS_X_VERSION_MIN_REQUIRED < MAC_OS_VERSION_12_0) || \
    (BUILDFLAG(IS_IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_15_0)
  base::apple::ScopedCFTypeRef<CFMutableArrayRef> chain(
      CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));
  const CFIndex chain_length = SecTrustGetCertificateCount(trust);
  for (CFIndex i = 0; i < chain_length; ++i) {
    CFArrayAppendValue(chain.get(), SecTrustGetCertificateAtIndex(trust, i));
  }
  return chain;

#else
  // The other logic paths should be used, this is just to make the compiler
  // happy.
  NOTREACHED_IN_MIGRATION();
  return base::apple::ScopedCFTypeRef<CFArrayRef>(nullptr);
#endif  // (BUILDFLAG(IS_MAC) && MAC_OS_X_VERSION_MIN_REQUIRED <
        // MAC_OS_VERSION_12_0)
        // || (BUILDFLAG(IS_IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED <
        // __IPHONE_15_0)
}

}  // namespace x509_util
}  // namespace net
