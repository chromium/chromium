// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_VIEW_CONTROLLER_HELPER_H_
#define IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_VIEW_CONTROLLER_HELPER_H_

#import <UIKit/UIKit.h>

namespace web {
class WebState;
}  // namespace web

// Helper for the BrowserViewController.
@interface BrowserViewControllerHelper : NSObject

// Returns true if the current tab of |webState| is currently loading.
- (BOOL)isToolbarLoading:(web::WebState*)webState;

// Returns true if |webState|'s current tab's URL is bookmarked, either by the
// user or by a managed bookmarks.
- (BOOL)isWebStateBookmarked:(web::WebState*)webState;

// Returns true if |webState|'s current tab's URL is bookmarked by the user;
// returns false if the URL is bookmarked only in managed bookmarks.
- (BOOL)isWebStateBookmarkedByUser:(web::WebState*)webState;

@end

#endif  // IOS_CHROME_BROWSER_UI_BROWSER_VIEW_BROWSER_VIEW_CONTROLLER_HELPER_H_
