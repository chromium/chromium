// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_ITEM_VIEW_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_ITEM_VIEW_H_

#import <UIKit/UIKit.h>

enum class SafetyCheckItemType;
enum class SafetyCheckItemLayoutType;
@class SafetyCheckState;

// A view to display an individual check state (list item) in the Safety Check
// (Magic Stack) module.
@interface SafetyCheckItemView : UIView

// Initialize a SafetyCheckItemView with the given `itemType` and `layoutType`.
- (instancetype)initWithItemType:(SafetyCheckItemType)itemType
                   andLayoutType:(SafetyCheckItemLayoutType)layoutType;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_SAFETY_CHECK_SAFETY_CHECK_ITEM_VIEW_H_
