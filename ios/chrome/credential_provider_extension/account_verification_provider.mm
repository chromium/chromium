// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/credential_provider_extension/account_verification_provider.h"

@implementation AccountVerificationProvider

- (void)validationIDForAccountID:(NSString*)accountID
               completionHandler:
                   (void (^)(NSString*, NSError*))completionHandler {
  // Default implementation always return nil.
  dispatch_async(dispatch_get_main_queue(), ^{
    completionHandler(nil, nil);
  });
}

- (void)validateValidationID:(NSString*)validationID
           completionHandler:(void (^)(BOOL, NSError*))completionHandler {
  // Default implementation always return true.
  dispatch_async(dispatch_get_main_queue(), ^{
    completionHandler(YES, nil);
  });
}

@end
