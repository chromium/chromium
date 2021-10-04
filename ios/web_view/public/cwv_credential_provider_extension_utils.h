// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_CREDENTIAL_PROVIDER_EXTENSION_UTILS_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_CREDENTIAL_PROVIDER_EXTENSION_UTILS_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

// Utils for implementing the iOS credential provider extension.
CWV_EXPORT
@interface CWVCredentialProviderExtensionUtils : NSObject

// Retrieves the password in the keychain matching |keychainIdentifier|.
// Returns nil if no password is associated with |keychainIdentifier|, or if
// |keychainIdentifier| is an empty string.
// This should only be used when implementing the credential provider extension.
+ (nullable NSString*)retrievePasswordForKeychainIdentifier:
    (NSString*)keychainIdentifier;

// Stores |password| into the keychain as an item retrievable by
// |keychainIdentifier|. |keychainIdentifier| must be a non-empty string.
// Returns BOOL indicating if store operation was successful.
// This should only be used when implementing the credential provider extension.
+ (BOOL)storePasswordForKeychainIdentifier:(NSString*)keychainIdentifier
                                  password:(NSString*)password;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_CREDENTIAL_PROVIDER_EXTENSION_UTILS_H_
