// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/cert_test_util.h"

namespace web {

base::apple::ScopedCFTypeRef<SecTrustRef> CreateServerTrustFromChain(
    NSArray* certs,
    NSString* host) {
  base::apple::ScopedCFTypeRef<SecTrustRef> scoped_result;
  if (certs.count == 0) {
    return scoped_result;
  }

  base::apple::ScopedCFTypeRef<SecPolicyRef> policy(
      SecPolicyCreateSSL(TRUE, static_cast<CFStringRef>(host)));
  SecTrustRef ref_result = nullptr;
  if (SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy.get(),
                                     &ref_result) == errSecSuccess) {
    scoped_result.reset(ref_result);
  }
  return scoped_result;
}

}  // namespace web
