// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_SECURITY_CRW_CERT_VERIFICATION_CONTROLLER_H_
#define IOS_WEB_SECURITY_CRW_CERT_VERIFICATION_CONTROLLER_H_

#import <Foundation/Foundation.h>

#include "base/mac/scoped_cftyperef.h"
#include "base/memory/ref_counted.h"
#include "ios/web/public/security/security_style.h"
#include "net/cert/cert_status_flags.h"

namespace net {
class X509Certificate;
}

namespace web {

class BrowserState;

// Accept policy for valid or invalid SSL cert.
typedef NS_ENUM(NSInteger, CertAcceptPolicy) {
  // Cert status can't be determined due to an error. Caller should reject the
  // load and show a net error page.
  CERT_ACCEPT_POLICY_NON_RECOVERABLE_ERROR = 0,
  // The cert is not valid. Caller may present an SSL warning and ask the user
  // if they want to proceed or reject the load.
  CERT_ACCEPT_POLICY_RECOVERABLE_ERROR_UNDECIDED_BY_USER,
  // The cert is not valid. However, the caller should proceed with the load
  // because the user has decided to proceed with this invalid cert.
  CERT_ACCEPT_POLICY_RECOVERABLE_ERROR_ACCEPTED_BY_USER,
  // The cert is valid. Caller should proceed with the load.
  CERT_ACCEPT_POLICY_ALLOW,
};

// Completion handler called by decideLoadPolicyForTrust:host:completionHandler.
typedef void (^PolicyDecisionHandler)(web::CertAcceptPolicy, net::CertStatus);
// Completion handler called by querySSLStatusForTrust:host:completionHandler:.
typedef void (^StatusQueryHandler)(web::SecurityStyle, net::CertStatus);

}  // namespace web

// Provides various cert verification API that can be used for blocking requests
// with bad SSL cert, presenting SSL interstitials and determining SSL status
// for Navigation Items. Must be used on UI thread.
@interface CRWCertVerificationController : NSObject

- (instancetype)init NS_UNAVAILABLE;

// Initializes CRWCertVerificationController with the given |browserState| which
// cannot be null and must outlive CRWCertVerificationController.
- (instancetype)initWithBrowserState:(web::BrowserState*)browserState
    NS_DESIGNATED_INITIALIZER;

// Decides the policy for the given |trust| and for the given |host| and calls
// |completionHandler| on completion. |completionHandler| is guaranteed to be
// called even if this object is deallocated. |host| should be in ASCII
// compatible form (e.g. for "http://名がドメイン.com", it should be
// "xn--v8jxj3d1dzdz08w.com"). |completionHandler| cannot be null and will be
// called asynchronously on the UI thread.
// Note: Certificate errors may be bypassed by calling
// |allowCert:forHost:status:| with the host, certificate, and certificate
// error to ignore.
- (void)decideLoadPolicyForTrust:(base::ScopedCFTypeRef<SecTrustRef>)trust
                            host:(NSString*)host
               completionHandler:(web::PolicyDecisionHandler)completionHandler;

// Asynchronously provides web::SecurityStyle and net::CertStatus for the given
// |trust| and |host|. |host| should be in ASCII compatible form.
// |completionHandler| is guaranteed to be called even if this object is
// deallocated.
// Note: The web::SecurityStyle determines whether the certificate is trusted.
// It is possible for an untrusted certificate to return a net::CertStatus with
// no errors if the cause could not be determined. Callers must handle this case
// gracefully.
- (void)querySSLStatusForTrust:(base::ScopedCFTypeRef<SecTrustRef>)trust
                          host:(NSString*)host
             completionHandler:(web::StatusQueryHandler)completionHandler;

// Records that |cert| is permitted to be used for |host| in future calls to
// |decideLoadPolicyForTrust:host:completionHandler:|. |host| should be in an
// ASCII-compatible form. Subsequent calls to
// |decideLoadPolicyForTrust:host:completionHandler:| for the same
// |cert|/|host| tuple and same |status| (or a subset of the given |status|)
// will return CERT_ACCEPT_POLICY_RECOVERABLE_ERROR_ACCEPTED_BY_USER.
- (void)allowCert:(scoped_refptr<net::X509Certificate>)cert
          forHost:(NSString*)host
           status:(net::CertStatus)status;

@end

#endif  // IOS_WEB_SECURITY_CRW_CERT_VERIFICATION_CONTROLLER_H_
