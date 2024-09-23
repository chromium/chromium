// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAD_TAB_UI_BUNDLED_SAD_TAB_VIEW_H_
#define IOS_CHROME_BROWSER_SAD_TAB_UI_BUNDLED_SAD_TAB_VIEW_H_

#import <UIKit/UIKit.h>

#include "base/ios/block_types.h"
#include "ios/web/public/navigation/navigation_manager.h"

@protocol ApplicationCommands;
@class SadTabView;

// Describes the mode of the Sad Tab, whether it should offer an attempt to
// reload content, or whether it should offer a way to provide feedback.
enum class SadTabViewMode {
  RELOAD = 0,  // A mode which allows the user to attempt a reload
  FEEDBACK,    // A mode which allows the user to provide feedback
};

@protocol SadTabViewDelegate
// Instructs the delegate to show Report An Issue UI.
- (void)sadTabViewShowReportAnIssue:(SadTabView*)sadTabView;

// Instructs the delegate to show Suggestions help page with the given URL.
- (void)sadTabView:(SadTabView*)sadTabView
    showSuggestionsPageWithURL:(const GURL&)URL;

// Instructs the delegate to reload this page.
- (void)sadTabViewReload:(SadTabView*)sadTabView;

@end

// The view used to show "sad tab" content to the user when WebState's renderer
// process crashes.
@interface SadTabView : UIView

- (instancetype)initWithMode:(SadTabViewMode)mode
                offTheRecord:(BOOL)offTheRecord NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithFrame:(CGRect)frame NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

// Determines the type of Sad Tab information that will be displayed.
@property(nonatomic, readonly) SadTabViewMode mode;

@property(nonatomic, weak) id<SadTabViewDelegate> delegate;

@end

// All UI elements present in view.
@interface SadTabView (UIElements)

// Displays the Sad Tab message.
@property(nonatomic, readonly) UITextView* messageTextView;

// Triggers a reload or feedback action.
@property(nonatomic, readonly) UIButton* actionButton;

@end

#endif  // IOS_CHROME_BROWSER_SAD_TAB_UI_BUNDLED_SAD_TAB_VIEW_H_
