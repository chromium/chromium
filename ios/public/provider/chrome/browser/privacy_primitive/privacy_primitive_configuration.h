// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_PRIVACY_PRIMITIVE_PRIVACY_PRIMITIVE_CONFIGURATION_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_PRIVACY_PRIMITIVE_PRIVACY_PRIMITIVE_CONFIGURATION_H_

#import <Foundation/Foundation.h>

@protocol SystemIdentity;

/// Configuration for launching a privacy primitive flow (ConsentKit).
@interface PrivacyPrimitiveConfiguration : NSObject

/// The identity for which the privacy primitive flow is displayed.
@property(nonatomic, strong) id<SystemIdentity> identity;

/// The flow ID associated with the privacy primitive.
@property(nonatomic, assign) NSInteger flowID;

/// The product ID associated with the privacy primitive.
@property(nonatomic, assign) NSInteger productID;

/// The product surface associated with the privacy primitive.
@property(nonatomic, assign) NSInteger productSurface;

/// A callback that will be used to open URLs requested by the privacy flow.
@property(nonatomic, copy) void (^openURLCallback)(NSURL* URL);

@end

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_PRIVACY_PRIMITIVE_PRIVACY_PRIMITIVE_CONFIGURATION_H_
