// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_CREDENTIAL_PROVIDER_MIGRATION_NOTIFIER_H_
#define IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_CREDENTIAL_PROVIDER_MIGRATION_NOTIFIER_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"

// The purpose of this class is to call the provided block as soon as
// credentials need to be migrated from the credential provider extension to
// the browser.
@interface CredentialProviderMigrationNotifier : NSObject

// Creating an instance of this class is made by the browser, in order to
// receive a notification (sent using the provided "block") that credentials
// need to be migrated from the credential provider extension to the browser.
- (instancetype)initWithBlock:(ProceduralBlock)block;

// This class method is used by the CPE to send a notification to the browser
// after credentials change or are created, so that a migration can be
// triggered.
+ (void)notifyMigrationNeeded;

@end

#endif  // IOS_CHROME_COMMON_CREDENTIAL_PROVIDER_CREDENTIAL_PROVIDER_MIGRATION_NOTIFIER_H_
