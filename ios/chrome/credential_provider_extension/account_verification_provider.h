// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_ACCOUNT_VERIFICATION_PROVIDER_H_
#define IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_ACCOUNT_VERIFICATION_PROVIDER_H_

#import <Foundation/Foundation.h>

// Provider for account verification. New methods should be added to protocol
// as @optional, until implemented in all replacement providers.
@protocol AccountVerificationProvider

// Completes with a validationID (or nil) that can be used with
// `validateValidationID:completionHandler:` at a later time.
- (void)validationIDForAccountID:(NSString*)accountID
               completionHandler:
                   (void (^)(NSString*, NSError*))completionHandler;

// Checks if the given `validationID` is valid, in the sense of
// credential provider extension should provide or ignore/delete credentials
// associated with this ID.  Completes with YES if account is valid or
// NO with or without error if it is not.
- (void)validateValidationID:(NSString*)validationID
           completionHandler:(void (^)(BOOL, NSError*))completionHandler;

@end

// Provider for account verification.
@interface AccountVerificationProvider : NSObject <AccountVerificationProvider>
@end

#endif  // IOS_CHROME_CREDENTIAL_PROVIDER_EXTENSION_ACCOUNT_VERIFICATION_PROVIDER_H_
