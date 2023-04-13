// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_COMMON_UI_UTIL_SDK_FORWARD_DECLARES_H_
#define IOS_CHROME_COMMON_UI_UTIL_SDK_FORWARD_DECLARES_H_

#import <UIKit/UIKit.h>

// TODO(crbug.com/1432568): Remove category when property no longer needs to be
// forward declared.
//  titleLineBreakMode has existed since iOS 15 with the implementation of
//  UIButtonConfiguration. It was only recently exposed in the iOS 16.3 SDK.
//  Until this property is exposed for all supported versions, we should forward
//  declare this property for versions that don't have the property exposed.
//  This property always exists at runtime in iOS 15.0+.
@interface UIButtonConfiguration (ForwardDeclare)

#if !defined(__IPHONE_16_3) || __IPHONE_OS_VERSION_MAX_ALLOWED < __IPHONE_16_3
@property(nonatomic, readwrite) NSLineBreakMode titleLineBreakMode;
#endif

@end

#endif  // IOS_CHROME_COMMON_UI_UTIL_SDK_FORWARD_DECLARES_H_
