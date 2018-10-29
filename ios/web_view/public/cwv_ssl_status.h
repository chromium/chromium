// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_SSL_STATUS_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_SSL_STATUS_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

// Web contents security style.
//
// Implementation comment: This enum mirrors web::SecurityStyle.
typedef NS_ENUM(NSInteger, CWVSecurityStyle) {
  // Security style of the web contents is not yet known. This is a temporary
  // state and at some point in the future security style will become
  // Unauthenticated, AuthenticationBroken or Authenticated.
  CWVSecurityStyleUnknown,
  // The authenticity of this object can not be determined, either because it
  // was retrieved using an unauthenticated protocol, such as HTTP or FTP, or it
  // was retrieved using a protocol that supports authentication, such as HTTPS,
  // but there were errors during transmission that render us uncertain to the
  // object's authenticity.
  CWVSecurityStyleUnauthenticated,
  // CWVWebView tried to retrieve this object in an authenticated manner but
  // were unable
  // to do so. Check CWVSSLStatus.certStatus for details about why it is broken.
  CWVSecurityStyleAuthenticationBroken,
  // CWVWebView successfully retrieved this object over an authenticated
  // protocol, such
  // as HTTPS.
  CWVSecurityStyleAuthenticated,
};

// Bit mask for the status of a SSL certificate.
//
// Implementation comment: This enum mirrors error statuses (not including
// non-error statuses) in //net/cert/cert_status_flags_list.h.
typedef NS_OPTIONS(NSInteger, CWVCertStatus) {
  CWVCertStatusCommonNameInvalid = 1 << 0,
  CWVCertStatusDateInvalid = 1 << 1,
  CWVCertStatusAuthorityInvalid = 1 << 2,
  CWVCertStatusNoRevocationMechanism = 1 << 4,
  CWVCertStatusUnableToCheckRevocation = 1 << 5,
  CWVCertStatusRevoked = 1 << 6,
  CWVCertStatusInvalid = 1 << 7,
  CWVCertStatusWeakSignatureAlgorithm = 1 << 8,
  CWVCertStatusNonUniqueName = 1 << 10,
  CWVCertStatusWeakKey = 1 << 11,
  CWVCertStatusPinnedKeyMissing = 1 << 13,
  CWVCertStatusNameConstraintViolation = 1 << 14,
  CWVCertStatusValidityTooLong = 1 << 15,
  CWVCertStatusCertificateTransparencyRequired = 1 << 24,
  CWVCertStatusSymantecLegacy = 1 << 25,
};

// SSL status of a page.
CWV_EXPORT
@interface CWVSSLStatus : NSObject

// Security style of the web contents presented in the web view. Not specific to
// any frame, and represents security information as a whole.
@property(nonatomic, readonly) CWVSecurityStyle securityStyle;

// A Boolean value indicating whether all resources on the page have been loaded
// through securely encrypted connections.
//
// If |securityStyle| is CWVSecurityStyleAuthenticated but
// |hasOnlySecureContent| is NO, it indicates that the page is in HTTPS but it
// contains "displayed" HTTP resources (e.g., images, CSS) aka. "mixed content"
// page. Note that all active mixed content (i.e., JavaScript) is blocked by
// CWVWebView. This property is always NO when |securityStyle| is not
// CWVSecurityStyleAuthenticated.
@property(nonatomic, readonly) BOOL hasOnlySecureContent;

// Status of the main frame's SSL certificate..
@property(nonatomic, readonly) CWVCertStatus certStatus;

- (instancetype)init NS_UNAVAILABLE;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_SSL_STATUS_H_
