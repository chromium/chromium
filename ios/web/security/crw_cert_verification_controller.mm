// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/security/crw_cert_verification_controller.h"

#import <memory>

#import "base/apple/foundation_util.h"
#import "base/check_op.h"
#import "base/functional/bind.h"
#import "base/ios/block_types.h"
#import "base/memory/ref_counted.h"
#import "base/strings/sys_string_conversions.h"
#import "base/task/thread_pool.h"
#import "ios/web/public/browser_state.h"
#import "ios/web/public/security/certificate_policy_cache.h"
#import "ios/web/public/thread/web_task_traits.h"
#import "ios/web/public/thread/web_thread.h"
#import "ios/web/security/wk_web_view_security_util.h"
#import "net/cert/cert_verify_proc_ios.h"
#import "net/cert/x509_util.h"
#import "net/cert/x509_util_apple.h"

using base::TaskShutdownBehavior;
using base::TaskTraits;
using web::WebThread;

@interface CRWCertVerificationController () {
  // Used to remember user exceptions to invalid certs.
  scoped_refptr<web::CertificatePolicyCache> _certPolicyCache;
}

// Returns cert status for the given `trust`.
- (net::CertStatus)certStatusFromTrustResult:(SecTrustResultType)trustResult
                                  trustError:
                                      (base::apple::ScopedCFTypeRef<CFErrorRef>)
                                          trustError;

// Decides the policy for the given `trust` which was rejected by iOS and the
// given `host` and calls `handler` on completion. Must be called on UI thread.
// `handler` can not be null and will be called on UI thread.
- (void)
    decideLoadPolicyForRejectedTrustResult:(SecTrustResultType)trustResult
                                trustError:
                                    (base::apple::ScopedCFTypeRef<CFErrorRef>)
                                        trustError
                               serverTrust:
                                   (base::apple::ScopedCFTypeRef<SecTrustRef>)
                                       trust
                                      host:(NSString*)host
                         completionHandler:(web::PolicyDecisionHandler)handler;

// Verifies the given `trust` using SecTrustRef API. `completionHandler` cannot
// be null and will be called on UI thread or never be called if the worker task
// can't start or complete. Must be called on UI thread.
- (void)verifyTrust:(base::apple::ScopedCFTypeRef<SecTrustRef>)trust
    completionHandler:
        (void (^)(SecTrustResultType,
                  base::apple::ScopedCFTypeRef<CFErrorRef>))completionHandler;

// Returns cert accept policy for the given SecTrust result. `trustResult` must
// not be for a valid cert. Must be called on IO thread.
- (web::CertAcceptPolicy)
    loadPolicyForRejectedTrustResult:(SecTrustResultType)trustResult
                          certStatus:(net::CertStatus)certStatus
                         serverTrust:(SecTrustRef)trust
                                host:(NSString*)host;

@end

@implementation CRWCertVerificationController

#pragma mark - Public

- (instancetype)initWithBrowserState:(web::BrowserState*)browserState {
  DCHECK(browserState);
  DCHECK_CURRENTLY_ON(WebThread::UI);
  self = [super init];
  if (self) {
    _certPolicyCache =
        web::BrowserState::GetCertificatePolicyCache(browserState);
  }
  return self;
}

- (void)decideLoadPolicyForTrust:
            (base::apple::ScopedCFTypeRef<SecTrustRef>)trust
                            host:(NSString*)host
               completionHandler:(web::PolicyDecisionHandler)completionHandler {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  DCHECK(completionHandler);

  [self verifyTrust:trust
      completionHandler:^(SecTrustResultType trustResult,
                          base::apple::ScopedCFTypeRef<CFErrorRef> trustError) {
        DCHECK_CURRENTLY_ON(WebThread::UI);
        if (trustResult == kSecTrustResultProceed ||
            trustResult == kSecTrustResultUnspecified) {
          completionHandler(web::CERT_ACCEPT_POLICY_ALLOW, net::CertStatus());
          return;
        }
        [self decideLoadPolicyForRejectedTrustResult:trustResult
                                          trustError:trustError
                                         serverTrust:trust
                                                host:host
                                   completionHandler:completionHandler];
      }];
}

- (void)querySSLStatusForTrust:(base::apple::ScopedCFTypeRef<SecTrustRef>)trust
                          host:(NSString*)host
             completionHandler:(web::StatusQueryHandler)completionHandler {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  DCHECK(completionHandler);

  [self verifyTrust:trust
      completionHandler:^(SecTrustResultType trustResult,
                          base::apple::ScopedCFTypeRef<CFErrorRef> trustError) {
        web::SecurityStyle securityStyle =
            web::GetSecurityStyleFromTrustResult(trustResult);

        net::CertStatus certStatus =
            [self certStatusFromTrustResult:trustResult trustError:trustError];
        completionHandler(securityStyle, certStatus);
      }];
}

#pragma mark - Private

- (net::CertStatus)certStatusFromTrustResult:(SecTrustResultType)trustResult
                                  trustError:
                                      (base::apple::ScopedCFTypeRef<CFErrorRef>)
                                          trustError {
  net::CertStatus certStatus = net::CertStatus();
  switch (trustResult) {
    case kSecTrustResultProceed:
    case kSecTrustResultUnspecified:
      break;
    case kSecTrustResultDeny:
      certStatus |= net::CERT_STATUS_AUTHORITY_INVALID;
      break;
    default:
      certStatus |= net::CertVerifyProcIOS::GetCertFailureStatusFromError(
          trustError.get());
      if (!net::IsCertStatusError(certStatus)) {
        certStatus |= net::CERT_STATUS_INVALID;
      }
  }
  return certStatus;
}

- (void)
    decideLoadPolicyForRejectedTrustResult:(SecTrustResultType)trustResult
                                trustError:
                                    (base::apple::ScopedCFTypeRef<CFErrorRef>)
                                        trustError
                               serverTrust:
                                   (base::apple::ScopedCFTypeRef<SecTrustRef>)
                                       trust
                                      host:(NSString*)host
                         completionHandler:(web::PolicyDecisionHandler)handler {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  DCHECK(handler);
  web::GetIOThreadTaskRunner({})
      ->PostTask(FROM_HERE, base::BindOnce(^{
                   // `loadPolicyForRejectedTrustResult:certStatus:serverTrust
                   // :host:` can only be called on IO thread.
                   net::CertStatus certStatus =
                       [self certStatusFromTrustResult:trustResult
                                            trustError:trustError];

                   web::CertAcceptPolicy policy =
                       [self loadPolicyForRejectedTrustResult:trustResult
                                                   certStatus:certStatus
                                                  serverTrust:trust.get()
                                                         host:host];

                   // TODO(crbug.com/40588591): This should use
                   // PostTask to post to WebThread::UI with
                   // BLOCK_SHUTDOWN once shutdown behaviors are
                   // supported on the UI thread. BLOCK_SHUTDOWN is
                   // necessary because WKWebView throws an exception
                   // if the completion handler doesn't run.
                   dispatch_async(dispatch_get_main_queue(), ^{
                     handler(policy, certStatus);
                   });
                 }));
}

- (void)verifyTrust:(base::apple::ScopedCFTypeRef<SecTrustRef>)trust
    completionHandler:
        (void (^)(SecTrustResultType,
                  base::apple::ScopedCFTypeRef<CFErrorRef>))completionHandler {
  DCHECK_CURRENTLY_ON(WebThread::UI);
  DCHECK(completionHandler);
  // SecTrustEvaluate performs trust evaluation synchronously, possibly making
  // network requests. The UI thread should not be blocked by that operation.
  base::ThreadPool::PostTask(
      FROM_HERE, {TaskShutdownBehavior::BLOCK_SHUTDOWN}, base::BindOnce(^{
        SecTrustResultType trustResult = kSecTrustResultInvalid;
        base::apple::ScopedCFTypeRef<CFErrorRef> trustError;
        bool isTrusted =
            SecTrustEvaluateWithError(trust.get(), trustError.InitializeInto());
        if (SecTrustGetTrustResult(trust.get(), &trustResult) != errSecSuccess)
          trustResult = kSecTrustResultInvalid;
        DCHECK_EQ(isTrusted, (trustResult == kSecTrustResultProceed ||
                              trustResult == kSecTrustResultUnspecified));
        // TODO(crbug.com/40588591): This should use PostTask to post to
        // WebThread::UI with BLOCK_SHUTDOWN once shutdown behaviors are
        // supported on the UI thread. BLOCK_SHUTDOWN is necessary because
        // WKWebView throws an exception if the completion handler doesn't run.
        dispatch_async(dispatch_get_main_queue(), ^{
          completionHandler(trustResult, trustError);
        });
      }));
}

- (web::CertAcceptPolicy)
    loadPolicyForRejectedTrustResult:(SecTrustResultType)trustResult
                          certStatus:(net::CertStatus)certStatus
                         serverTrust:(SecTrustRef)trust
                                host:(NSString*)host {
  DCHECK_CURRENTLY_ON(WebThread::IO);
  DCHECK_NE(web::SECURITY_STYLE_AUTHENTICATED,
            web::GetSecurityStyleFromTrustResult(trustResult));

  if (trustResult != kSecTrustResultRecoverableTrustFailure ||
      SecTrustGetCertificateCount(trust) == 0) {
    // Trust result is not recoverable or leaf cert is missing.
    return web::CERT_ACCEPT_POLICY_NON_RECOVERABLE_ERROR;
  }

  // Check if user has decided to proceed with this bad cert.
  base::apple::ScopedCFTypeRef<CFArrayRef> certificateChain(
      SecTrustCopyCertificateChain(trust));
  SecCertificateRef secCertificate =
      base::apple::CFCastStrict<SecCertificateRef>(
          CFArrayGetValueAtIndex(certificateChain.get(), 0));
  scoped_refptr<net::X509Certificate> leafCert =
      net::x509_util::CreateX509CertificateFromSecCertificate(
          base::apple::ScopedCFTypeRef<SecCertificateRef>(
              secCertificate, base::scoped_policy::RETAIN),
          {});

  if (!leafCert)
    return web::CERT_ACCEPT_POLICY_NON_RECOVERABLE_ERROR;

  web::CertPolicy::Judgment judgment = _certPolicyCache->QueryPolicy(
      leafCert.get(), base::SysNSStringToUTF8(host), certStatus);

  return (judgment == web::CertPolicy::ALLOWED)
             ? web::CERT_ACCEPT_POLICY_RECOVERABLE_ERROR_ACCEPTED_BY_USER
             : web::CERT_ACCEPT_POLICY_RECOVERABLE_ERROR_UNDECIDED_BY_USER;
}

@end
