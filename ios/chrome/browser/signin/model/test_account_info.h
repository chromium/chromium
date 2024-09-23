// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SIGNIN_MODEL_TEST_ACCOUNT_INFO_H_
#define IOS_CHROME_BROWSER_SIGNIN_MODEL_TEST_ACCOUNT_INFO_H_

#import <Foundation/Foundation.h>

#import <string>

@interface TestAccountInfo : NSObject <NSSecureCoding>

// Encodes `identities` into a string, using NSKeyedArchiver.
+ (std::string)encodeTestAccountInfosToBase64:
    (NSArray<TestAccountInfo*>*)identities;
// Returns a list of TestAccountInfo encoded using
// `encodeIdentitiesToBase64:`.
+ (NSArray<TestAccountInfo*>*)testAccountInfosFromBase64String:
    (const std::string&)string;

// Returns the default values for capabilities when creating a TestAccountInfo
// instance. This is related to `AccountCapabilities`.
// @YES: the capability is true.
// @NO: the capability is false.
// No value: the capability is unknown.
+ (NSDictionary<NSString*, NSNumber*>*)defaultCapabilityValues;

// Returns a test account.
+ (instancetype)testAccountInfo1;
// Returns a second test account.
+ (instancetype)testAccountInfo2;
// Returns a third test account.
+ (instancetype)testAccountInfo3;
// Returns a forth test account.
+ (instancetype)testAccountInfo4;
// Returns a managed test account.
+ (instancetype)managedTestAccountInfo;

// Calls `[self testAccountInfoWithUserEmail:userEmail
//                                    gaiaID:nil]`
+ (instancetype)testAccountInfoWithUserEmail:(NSString*)userEmail;

// Calls `[[self alloc] initWithUserEmail:userEmail
//                                 gaiaID:gaiaID
//                           userFullName:nil
//                          userGivenName:nil
//                           capabilities:nil]`
+ (instancetype)testAccountInfoWithUserEmail:(NSString*)userEmail
                                      gaiaID:(NSString*)gaiaID;

@property(strong, nonatomic, strong, readonly) NSString* gaiaID;
@property(strong, nonatomic, strong, readonly) NSString* userEmail;
@property(strong, nonatomic, strong, readonly) NSString* userFullName;
@property(strong, nonatomic, strong, readonly) NSString* userGivenName;
// List capabilities related to `AccountCapabilities`.
// @YES: the capability is true.
// @NO: the capability is false.
// No value: the capability is unknown.
@property(strong, nonatomic, strong, readonly)
    NSDictionary<NSString*, NSNumber*>* capabilities;

// Initialises a TestAccountInfo.
// `userEmail`: cannot be `nil` or empty.
// `gaiaID`: if is nil or empty, the gaia is the email address (with `@`
// replaced by `_`).
// `userFullName` and `userGivenName`: if empty or nil, the name of the email
// address is used as the full name or/and the given name.
// `capabilities`: if is nil, `+[TestAccountInfo defaultCapabilityValues]` is
// used.
- (instancetype)initWithUserEmail:(NSString*)userEmail
                           gaiaID:(NSString*)gaiaID
                     userFullName:(NSString*)userFullName
                    userGivenName:(NSString*)userGivenName
                     capabilities:
                         (NSDictionary<NSString*, NSNumber*>*)capabilities
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_SIGNIN_MODEL_TEST_ACCOUNT_INFO_H_
