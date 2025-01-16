// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_CONSTANTS_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_CONSTANTS_H_

#import <UIKit/UIKit.h>

extern NSString* const kMostVisitedSectionIdentifier;
extern NSString* const kMagicStackSectionIdentifier;
extern NSString* const kMagicStackEditSectionIdentifier;

// The max height of the modules.
extern const int kModuleMaxHeight;

// The spacing between modules in the Magic Stack.
extern const CGFloat kMagicStackSpacing;

// The reduction in width of MagicStack modules from NTP modules. This
// reduction allows the next module to peek in from the side.
extern const CGFloat kMagicStackPeekInset;
extern const CGFloat kMagicStackPeekInsetLandscape;

// The bottom spacing underneath the Magic Stack.
extern const float kBottomMagicStackPadding;

// The minimum scroll velocity in order to swipe between modules in the Magic
// Stack.
extern const float kMagicStackMinimumPaginationScrollVelocity;

// The size configs of the Magic Stack edit button.
extern const float kMagicStackEditButtonWidth;
extern const float kMagicStackEditButtonIconPointSize;

// Margin spacing between Magic Stack Edit button and horizontal neighboring
// views.
extern const float kMagicStackEditButtonMargin;

// Represents the Edit Button Container in the Magic Stack.
extern NSString* const kMagicStackEditButtonContainerAccessibilityIdentifier;

// Represents the Edit Button in the Magic Stack.
extern NSString* const kMagicStackEditButtonAccessibilityIdentifier;

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_MAGIC_STACK_MAGIC_STACK_CONSTANTS_H_
