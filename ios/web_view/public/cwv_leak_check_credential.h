// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_LEAK_CHECK_CREDENTIAL_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_LEAK_CHECK_CREDENTIAL_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

@class CWVPassword;

// CWVLeakCheckCredential represents a credential to be checked with a leak
// check service. It implements isEqual/hash/NSCopying so identical credentials
// can be deduplicated and stored in dictionaries easily. Use the helper
// initializers to created canonical credentials for the most efficient
// deduplication.
CWV_EXPORT
@interface CWVLeakCheckCredential : NSObject <NSCopying>

- (instancetype)init NS_UNAVAILABLE;

// Creates a canonical credential from a CWVPassword so objects that would be
// seen as identical by a leak checking service are equivalent objects as well.
+ (CWVLeakCheckCredential*)canonicalLeakCheckCredentialWithPassword:
    (CWVPassword*)password;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_LEAK_CHECK_CREDENTIAL_H_
