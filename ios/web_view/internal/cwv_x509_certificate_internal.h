// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_INTERNAL_CWV_X509_CERTIFICATE_INTERNAL_H_
#define IOS_WEB_VIEW_INTERNAL_CWV_X509_CERTIFICATE_INTERNAL_H_

#import "ios/web_view/public/cwv_x509_certificate.h"

#include "base/memory/scoped_refptr.h"

namespace net {
class X509Certificate;
}  // namespace net

NS_ASSUME_NONNULL_BEGIN

@interface CWVX509Certificate ()

// Creates CWVX509Certificate which wraps |internalCertificate|.
- (instancetype)initWithInternalCertificate:
    (scoped_refptr<net::X509Certificate>)internalCertificate
    NS_DESIGNATED_INITIALIZER;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_INTERNAL_CWV_X509_CERTIFICATE_INTERNAL_H_
