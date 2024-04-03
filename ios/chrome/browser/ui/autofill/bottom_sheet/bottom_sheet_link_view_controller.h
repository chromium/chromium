// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_BOTTOM_SHEET_LINK_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_BOTTOM_SHEET_LINK_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class CrURL;
@protocol BottomSheetLinkViewControllerPresentationDelegate;

namespace web {
class BrowserState;
}  // namespace web

// A view controller for displaying legal message links (i.e. the content) from
// the payments bottom sheets (e.g. virtual card enrollment bottom sheet).
@interface BottomSheetLinkViewController : UIViewController

// Initializes this view controller including a web state to display link
// content.
- (instancetype)initWithBrowserState:(web::BrowserState*)browserState
                               title:(NSString*)title;

// Displays the given url in the (internal) web state.
- (void)openURL:(CrURL*)url;

// This delegate is responsible for dismissing us when requested.
@property(weak, nonatomic) id<BottomSheetLinkViewControllerPresentationDelegate>
    presentationDelegate;

@end

#endif  // IOS_CHROME_BROWSER_UI_AUTOFILL_BOTTOM_SHEET_BOTTOM_SHEET_LINK_VIEW_CONTROLLER_H_
