// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_COMMON_CRW_INPUT_VIEW_PROVIDER_H_
#define IOS_WEB_COMMON_CRW_INPUT_VIEW_PROVIDER_H_

#import <UIKit/UIKit.h>

// Subset of UIResponder's APIs. Includes APIs related to inputView and
// inputAccessoryView.
@protocol CRWResponderInputView <NSObject>

@optional
// APIs to show custom input views. These mimic the ones in UIResponder. Refer
// to Apple's documentation for more information.
- (UIView*)inputView;
- (UIInputViewController*)inputViewController;
- (UIView*)inputAccessoryView;
- (UIInputViewController*)inputAccessoryViewController;

@end

// Any object that adopts this protocol can provide an id<CRWResponderInputView>
// to show custom input views.
@protocol CRWInputViewProvider <NSObject>

// The actual object implementing the input view methods.
@property(nonatomic, readonly) id<CRWResponderInputView> responderInputView;

@end

#endif  // IOS_WEB_COMMON_CRW_INPUT_VIEW_PROVIDER_H_
