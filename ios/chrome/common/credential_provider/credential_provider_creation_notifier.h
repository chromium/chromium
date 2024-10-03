// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_CREDENTIAL_PROVIDER_CREATION_NOTIFIER_H_
#define IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_CREDENTIAL_PROVIDER_CREATION_NOTIFIER_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"

// The purpose of this class is to call the provided block as soon as a new
// credential has been created by the credential provider extension.
@interface CredentialProviderCreationNotifier : NSObject <NSFilePresenter>

- (instancetype)initWithBlock:(ProceduralBlock)block;

@end

#endif  // IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_CREDENTIAL_PROVIDER_CREATION_NOTIFIER_H_
