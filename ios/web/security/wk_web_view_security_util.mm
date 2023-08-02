// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/security/wk_web_view_security_util.h"

#import "base/mac/foundation_util.h"
#import "base/mac/scoped_cftyperef.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "net/cert/x509_certificate.h"
#import "net/cert/x509_util_apple.h"
#import "net/ssl/ssl_info.h"

namespace web {

// These keys were determined by inspecting userInfo dict of an SSL error.
NSString* const kNSErrorPeerCertificateChainKey =
    @"NSErrorPeerCertificateChainKey";
NSString* const kNSErrorFailingURLKey = @"NSErrorFailingURLKey";
}

namespace {

// Maps NSError code to net::CertStatus.
net::CertStatus GetCertStatusFromNSErrorCode(NSInteger code) {
  switch (code) {
    // Regardless of real certificate problem the system always returns
    // NSURLErrorServerCertificateUntrusted. The mapping is done in case this
    // bug is fixed (rdar://18517043).
    case NSURLErrorServerCertificateUntrusted:
    case NSURLErrorSecureConnectionFailed:
    case NSURLErrorServerCertificateHasUnknownRoot:
    case NSURLErrorClientCertificateRejected:
    case NSURLErrorClientCertificateRequired:
      return net::CERT_STATUS_INVALID;
    case NSURLErrorServerCertificateHasBadDate:
    case NSURLErrorServerCertificateNotYetValid:
      return net::CERT_STATUS_DATE_INVALID;
  }
  NOTREACHED();
  return 0;
}

}  // namespace

namespace web {

scoped_refptr<net::X509Certificate> CreateCertFromChain(NSArray* certs) {
  if (certs.count == 0)
    return nullptr;
  std::vector<base::ScopedCFTypeRef<SecCertificateRef>> intermediates;
  for (NSUInteger i = 1; i < certs.count; i++) {
    base::ScopedCFTypeRef<SecCertificateRef> cert(
        (__bridge SecCertificateRef)certs[i], base::scoped_policy::RETAIN);
    intermediates.push_back(cert);
  }
  base::ScopedCFTypeRef<SecCertificateRef> root_cert(
      (__bridge SecCertificateRef)certs[0], base::scoped_policy::RETAIN);
  return net::x509_util::CreateX509CertificateFromSecCertificate(
      std::move(root_cert), intermediates);
}

scoped_refptr<net::X509Certificate> CreateCertFromTrust(SecTrustRef trust) {
  if (!trust)
    return nullptr;

  CFIndex cert_count = SecTrustGetCertificateCount(trust);
  if (cert_count == 0) {
    // At the moment there is no API which allows trust creation w/o certs.
    return nullptr;
  }

  std::vector<base::ScopedCFTypeRef<SecCertificateRef>> intermediates;

  // TODO(crbug.com/1418068): Remove available and compile guard after minimum
  // version required is >= iOS 15.
  if (@available(iOS 15.0, *)) {
    base::ScopedCFTypeRef<CFArrayRef> certificateChain(
        SecTrustCopyCertificateChain(trust));
    for (CFIndex i = 1; i < cert_count; i++) {
      SecCertificateRef secCertificate =
          base::mac::CFCastStrict<SecCertificateRef>(
              CFArrayGetValueAtIndex(certificateChain, i));
      intermediates.emplace_back(secCertificate, base::scoped_policy::RETAIN);
    }
    SecCertificateRef secCertificate =
        base::mac::CFCastStrict<SecCertificateRef>(
            CFArrayGetValueAtIndex(certificateChain, 0));
    return net::x509_util::CreateX509CertificateFromSecCertificate(
        base::ScopedCFTypeRef<SecCertificateRef>(secCertificate,
                                                 base::scoped_policy::RETAIN),
        intermediates);
  }
#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_15_0
  else {
    for (CFIndex i = 1; i < cert_count; i++) {
      intermediates.emplace_back(SecTrustGetCertificateAtIndex(trust, i),
                                 base::scoped_policy::RETAIN);
    }
    return net::x509_util::CreateX509CertificateFromSecCertificate(
        base::ScopedCFTypeRef<SecCertificateRef>(
            SecTrustGetCertificateAtIndex(trust, 0),
            base::scoped_policy::RETAIN),
        intermediates);
  }
#else
  // TODO(crbug.com/1418068): Added to make the compiler happy and should be
  // removed when compile and available guards are cleaned up.
  NOTREACHED();
  return net::x509_util::CreateX509CertificateFromSecCertificate(
      base::ScopedCFTypeRef<SecCertificateRef>(nullptr), intermediates);
#endif  // __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_15_0
}

base::ScopedCFTypeRef<SecTrustRef> CreateServerTrustFromChain(NSArray* certs,
                                                              NSString* host) {
  base::ScopedCFTypeRef<SecTrustRef> scoped_result;
  if (certs.count == 0)
    return scoped_result;

  base::ScopedCFTypeRef<SecPolicyRef> policy(
      SecPolicyCreateSSL(TRUE, static_cast<CFStringRef>(host)));
  SecTrustRef ref_result = nullptr;
  if (SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy,
                                     &ref_result) == errSecSuccess) {
    scoped_result.reset(ref_result);
  }
  return scoped_result;
}

void EnsureFutureTrustEvaluationSucceeds(SecTrustRef trust) {
  base::ScopedCFTypeRef<CFDataRef> exceptions(SecTrustCopyExceptions(trust));
  SecTrustSetExceptions(trust, exceptions);
}

BOOL IsWKWebViewSSLCertError(NSError* error) {
  if (![error.domain isEqualToString:NSURLErrorDomain]) {
    return NO;
  }

  switch (error.code) {
    case NSURLErrorServerCertificateHasBadDate:
    case NSURLErrorServerCertificateUntrusted:
    case NSURLErrorServerCertificateHasUnknownRoot:
    case NSURLErrorServerCertificateNotYetValid:
      return YES;
    case NSURLErrorSecureConnectionFailed:
      // Although the finer-grained errors above exist, iOS never uses them
      // and instead signals NSURLErrorSecureConnectionFailed for both
      // certificate failures and other SSL connection failures. Instead, check
      // if the error has a certificate attached (crbug.com/539735).
      return [error.userInfo[web::kNSErrorPeerCertificateChainKey] count] > 0;
    default:
      return NO;
  }
}

void GetSSLInfoFromWKWebViewSSLCertError(NSError* error,
                                         net::SSLInfo* ssl_info) {
  DCHECK(IsWKWebViewSSLCertError(error));
  scoped_refptr<net::X509Certificate> cert = web::CreateCertFromChain(
      error.userInfo[web::kNSErrorPeerCertificateChainKey]);
  ssl_info->cert = cert;
  ssl_info->unverified_cert = cert;
  ssl_info->cert_status = cert ? GetCertStatusFromNSErrorCode(error.code)
                               : net::CERT_STATUS_INVALID;
}

SecurityStyle GetSecurityStyleFromTrustResult(SecTrustResultType result) {
  switch (result) {
    case kSecTrustResultInvalid:
      return SECURITY_STYLE_UNKNOWN;
    case kSecTrustResultProceed:
    case kSecTrustResultUnspecified:
      return SECURITY_STYLE_AUTHENTICATED;
    case kSecTrustResultDeny:
    case kSecTrustResultRecoverableTrustFailure:
    case kSecTrustResultFatalTrustFailure:
    case kSecTrustResultOtherError:
      return SECURITY_STYLE_AUTHENTICATION_BROKEN;
  }
}

}  // namespace web
