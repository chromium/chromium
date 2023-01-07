// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_X509_CERTIFICATE_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_X509_CERTIFICATE_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

// CWVX509Certificate represents a X.509 certificate, which is comprised a
// particular identity or end-entity certificate, such as an SSL server
// identity or an SSL client certificate, and zero or more intermediate
// certificates that may be used to build a path to a root certificate.
CWV_EXPORT
@interface CWVX509Certificate : NSObject

- (instancetype)init NS_UNAVAILABLE;

// A name that can be used to represent the issuer or an empty string.
@property(nonatomic, readonly) NSString* issuerDisplayName;

// A date after which the certificate is invalid.
@property(nonatomic, readonly) NSDate* validExpiry;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_X509_CERTIFICATE_H_
