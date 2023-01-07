// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_MODULE_CONTAINER_H_
#define IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_MODULE_CONTAINER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ui/content_suggestions/content_suggestions_constants.h"

// Container View for a module in the Content Suggestions.
@interface ContentSuggestionsModuleContainer : UIView

// Initializes the view with a `contentView` content that is showing the Content
// Suggestions `type`.
- (instancetype)initWithContentView:(UIView*)contentView
                         moduleType:(ContentSuggestionsModuleType)type;

- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// If YES, the module will show a placeholder UI.
@property(nonatomic, assign) BOOL isPlaceholder;

// Returns the intrisic height for the entire module, including its content.
- (CGFloat)calculateIntrinsicHeight;

@end

#endif  // IOS_CHROME_BROWSER_UI_CONTENT_SUGGESTIONS_CELLS_CONTENT_SUGGESTIONS_MODULE_CONTAINER_H_
