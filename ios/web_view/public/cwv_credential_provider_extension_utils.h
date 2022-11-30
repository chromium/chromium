// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_VIEW_PUBLIC_CWV_CREDENTIAL_PROVIDER_EXTENSION_UTILS_H_
#define IOS_WEB_VIEW_PUBLIC_CWV_CREDENTIAL_PROVIDER_EXTENSION_UTILS_H_

#import <Foundation/Foundation.h>

#import "cwv_export.h"

NS_ASSUME_NONNULL_BEGIN

// Utils for implementing the iOS credential provider extension.
// The credential provider extension is usually resource constrained, and so
// should limit API usage to those defined in this class.
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

// Generates a random password.
// |host| For example "www.chromium.org". This is used to look up the password
// requirements like length, upper/lower case, symbols, etc.
// |APIKey| Used to access the password spec API.
// |completionHandler| Will be called asynchronously with a generated password.
+ (void)generateRandomPasswordForHost:(NSString*)host
                               APIKey:(NSString*)APIKey
                    completionHandler:(void (^)(NSString* generatedPassword))
                                          completionHandler;

@end

NS_ASSUME_NONNULL_END

#endif  // IOS_WEB_VIEW_PUBLIC_CWV_CREDENTIAL_PROVIDER_EXTENSION_UTILS_H_
