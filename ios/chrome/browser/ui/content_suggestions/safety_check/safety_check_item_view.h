// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_ITEM_VIEW_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_ITEM_VIEW_H_

#import <UIKit/UIKit.h>

enum class SafetyCheckItemType;
enum class SafetyCheckItemLayoutType;
@class SafetyCheckItemView;
@class SafetyCheckState;

// A protocol for handling `SafetyCheckItemView` taps.
@protocol SafetyCheckItemViewTapDelegate
// Indicates that the user has tapped the given `view`.
- (void)didTapSafetyCheckItemView:(SafetyCheckItemView*)view;
@end

// A view to display an individual check state (list item) in the Safety Check
// (Magic Stack) module.
@interface SafetyCheckItemView : UIView

// Convenience initializer for creating a SafetyCheckItemView with the given
// `itemType` and `layoutType`, but without specific insecure credentials
// information.
- (instancetype)initWithItemType:(SafetyCheckItemType)itemType
                      layoutType:(SafetyCheckItemLayoutType)layoutType;

// Initialize a SafetyCheckItemView with the given `itemType`, `layoutType`,
// `weakPasswordsCount`, `reusedPasswordsCount`, and
// `compromisedPasswordsCount`.
- (instancetype)initWithItemType:(SafetyCheckItemType)itemType
                      layoutType:(SafetyCheckItemLayoutType)layoutType
              weakPasswordsCount:(NSInteger)weakPasswordsCount
            reusedPasswordsCount:(NSInteger)reusedPasswordsCount
       compromisedPasswordsCount:(NSInteger)compromisedPasswordsCount;

// Indicates the type of item.
@property(nonatomic, readonly) SafetyCheckItemType itemType;

// The object that should receive a message when this view is tapped.
@property(nonatomic, weak) id<SafetyCheckItemViewTapDelegate> tapDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_ITEM_VIEW_H_
