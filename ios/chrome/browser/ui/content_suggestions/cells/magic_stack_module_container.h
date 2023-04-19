// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_MAGIC_STACK_MODULE_CONTAINER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_MAGIC_STACK_MODULE_CONTAINER_H_

#import <UIKit/UIKit.h>

enum class ContentSuggestionsModuleType;

// Container View for a module in the Magic Stack.
@interface MagicStackModuleContainer : UIView

// Initialize and configure for `type`.
- (instancetype)initWithType:(ContentSuggestionsModuleType)type;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Returns the module width depending on the horizontal `traitCollection`.
+ (CGFloat)moduleWidthForHorizontalTraitCollection:
    (UITraitCollection*)traitCollection;

// Returns the title string for the module.
- (NSString*)titleString;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_MAGIC_STACK_MODULE_CONTAINER_H_
