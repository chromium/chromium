// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POLICY_POLICY_APP_INTERFACE_H_
#define IOS_CHROME_BROWSER_POLICY_POLICY_APP_INTERFACE_H_

#import <Foundation/Foundation.h>

@interface PolicyAppInterface : NSObject

// Returns a JSON-encoded representation of the value for the given |policyKey|.
// Looks for the policy in the platform policy provider under the CHROME policy
// namespace.
+ (NSString*)valueForPlatformPolicy:(NSString*)policyKey;

// Sets the value of the policy with the |policyKey| key to the given value. The
// value must be serialized to JSON.
+ (void)setPolicyValue:(NSString*)jsonValue forKey:(NSString*)policyKey;

// Clear all policy values.
+ (void)clearPolicies;

// Returns YES if the given |URL| is blocked by the URLBlocklist and
// URLAllowlist policies.
+ (BOOL)isURLBlocked:(NSString*)URL;

@end

#endif  // IOS_CHROME_BROWSER_POLICY_POLICY_APP_INTERFACE_H_
