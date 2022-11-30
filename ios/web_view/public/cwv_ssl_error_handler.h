// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_SSL_ERROR_HANDLER_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_SSL_ERROR_HANDLER_H_

#import <Foundation/Foundation.h>

#import "cwv_cert_status.h"
#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

// Used to handle SSL errors like expired certificates, self signed, etc.
CWV_EXPORT
@interface CWVSSLErrorHandler : NSObject

// The URL of the failing page.
@property(nonatomic, readonly) NSURL* URL;

// Contains details about the SSL error.
@property(nonatomic, readonly) NSError* error;

// Whether or not |overrideErrorAndReloadPage| can be used to ignore the error
// and proceed to the page.
@property(nonatomic, readonly) BOOL overridable;

// The status of the SSL certificate.
@property(nonatomic, readonly) CWVCertStatus certStatus;

- (instancetype)init NS_UNAVAILABLE;

// Call to display an error page with |HTML|.
// This will no op after the first call.
- (void)displayErrorPageWithHTML:(NSString*)HTML;

// Call to override the error and reload the page.
// No op unless |overridable| is YES.
- (void)overrideErrorAndReloadPage;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_SSL_ERROR_HANDLER_H_
