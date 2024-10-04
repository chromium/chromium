// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_CREDENTIAL_PROVIDER_CREATION_NOTIFIER_H_
#define IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_CREDENTIAL_PROVIDER_CREATION_NOTIFIER_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"

// The purpose of this class is to call the provided block as soon as a new
// credential has been created by the credential provider extension.
@interface CredentialProviderCreationNotifier : NSObject

// Creating an instance of this class is made by Chrome, in order to receive a
// notification (sent using the provided "block") that a new credential has
// been created by the credential provider extension.
- (instancetype)initWithBlock:(ProceduralBlock)block;

// This class method is used by the CPE to send a notification to Chrome after
// having created a new credential.
+ (void)notifyCredentialCreated;

@end

#endif  // IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_CREDENTIAL_PROVIDER_CREATION_NOTIFIER_H_
