// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_CWV_SSL_STATUS_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_CWV_SSL_STATUS_INTERNAL_H_

#include "ios/web/public/security/ssl_status.h"
#import "ios/web_view/public/cwv_ssl_status.h"

NS_ASSUME_NONNULL_BEGIN

// Converts net::CertStatus to CWVCertStatus.
CWVCertStatus CWVCertStatusFromNetCertStatus(net::CertStatus cert_status);

@interface CWVSSLStatus ()

// Creates CWVSSLStatus which wraps |internalStatus|.
- (instancetype)initWithInternalStatus:(const web::SSLStatus&)internalStatus
    NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_CWV_SSL_STATUS_INTERNAL_H_
