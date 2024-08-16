// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_TEST_ACCOUNT_INFO_BUILDER_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_TEST_ACCOUNT_INFO_BUILDER_H_

#import <Foundation/Foundation.h>

#import "components/signin/public/identity_manager/tribool.h"

@class TestAccountInfo;

@interface TestAccountInfoBuilder : NSObject

@property(nonatomic, strong) NSString* userEmail;
@property(nonatomic, strong) NSString* userFullName;
@property(nonatomic, strong) NSString* userGivenName;

- (instancetype)initWithTestAccountInfo:(TestAccountInfo*)testAccountInfo
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Sets a capability with a value.
- (void)setCapabilityValue:(signin::Tribool)value forName:(NSString*)name;
// Resets all capability values to unknown.
- (void)resetCapabilityToUnkownValues;
// Return an immutable TestAccountInfo instance.
- (TestAccountInfo*)build;

@end

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_TEST_ACCOUNT_INFO_BUILDER_H_
