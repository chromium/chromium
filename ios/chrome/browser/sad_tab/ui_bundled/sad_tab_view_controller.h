// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SAD_TAB_UI_BUNDLED_SAD_TAB_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_SAD_TAB_UI_BUNDLED_SAD_TAB_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

class GURL;
@protocol OverscrollActionsControllerDelegate;
@class SadTabViewController;

@protocol SadTabViewControllerDelegate<NSObject>
// Instructs the delegate to show Report An Issue UI.
- (void)sadTabViewControllerShowReportAnIssue:
    (SadTabViewController*)sadTabViewController;

// Instructs the delegate to show Suggestions help page with the given URL.
- (void)sadTabViewController:(SadTabViewController*)sadTabViewController
    showSuggestionsPageWithURL:(const GURL&)URL;

// Instructs the delegate to reload this page.
- (void)sadTabViewControllerReload:(SadTabViewController*)sadTabViewController;
@end

// View controller that displays a SadTab view.
@interface SadTabViewController : UIViewController

@property(nonatomic, weak) id<SadTabViewControllerDelegate> delegate;

// Required to support Overscroll Actions UI, which is displayed when Sad Tab is
// pulled down.
@property(nonatomic, weak) id<OverscrollActionsControllerDelegate>
    overscrollDelegate;

// YES if page load for this URL has failed more than once.
@property(nonatomic, assign) BOOL repeatedFailure;

// YES if browsing mode is off the record.
@property(nonatomic, assign) BOOL offTheRecord;

@end

// All UI elements present in view controller's view.
@interface SadTabViewController (UIElements)

// Displays the Sad Tab message.
@property(nonatomic, readonly) UITextView* messageTextView;

// Triggers a reload or feedback action.
@property(nonatomic, readonly) UIButton* actionButton;

@end

#endif  // IOS_CHROME_BROWSER_SAD_TAB_UI_BUNDLED_SAD_TAB_VIEW_CONTROLLER_H_
