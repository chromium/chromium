// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_TEST_CERT_TEST_UTIL_H_
#define IOS_WEB_TEST_CERT_TEST_UTIL_H_

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#include "base/apple/scoped_cftyperef.h"

namespace web {

// Creates a server trust object from an array of SecCertificateRef objects.
// Returns null if `certs` is nil or empty.
base::apple::ScopedCFTypeRef<SecTrustRef> CreateServerTrustFromChain(
    NSArray* certs,
    NSString* host);

}  // namespace web

#endif  // IOS_WEB_TEST_CERT_TEST_UTIL_H_
