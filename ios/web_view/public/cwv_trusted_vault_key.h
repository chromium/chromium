// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_TRUSTED_VAULT_KEY_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_TRUSTED_VAULT_KEY_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

// Represents a shared trusted vault encryption key paired with its version.
CWV_EXPORT
@interface CWVTrustedVaultKey : NSObject

- (instancetype)initWithKeyMaterial:(NSData*)keyMaterial
                            version:(NSInteger)version
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Opaque key material bytes.
@property(nonatomic, copy, readonly) NSData* keyMaterial;

// Version (epoch) of the key.
@property(nonatomic, assign, readonly) NSInteger version;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_TRUSTED_VAULT_KEY_H_
