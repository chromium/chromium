// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SHARED_UI_UTIL_CUSTOM_UI_TRAIT_ACCESSOR_H_
#define IOS_CHROME_BROWSER_SHARED_UI_UTIL_CUSTOM_UI_TRAIT_ACCESSOR_H_

#import <UIKit/UIKit.h>

// Wrapper class to allow extension of the `UIMutableTraits` protocol for type
// safety.
@interface CustomUITraitAccessor : NSObject

- (instancetype)initWithMutableTraits:(id<UIMutableTraits>)mutableTraits
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// The underlying mutable traits instance, for subclasses to use.
@property(nonatomic, weak, readonly) id<UIMutableTraits> mutableTraits;

@end

#endif  // IOS_CHROME_BROWSER_SHARED_UI_UTIL_CUSTOM_UI_TRAIT_ACCESSOR_H_
