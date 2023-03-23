// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_ACTION_HANDLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_ACTION_HANDLER_DELEGATE_H_

#import <UIKit/UIKit.h>

@protocol PopupMenuItem;
@class TableViewItem;
namespace web {
class WebState;
}

// Delegate for the PopupMenuActionHandler.
@protocol PopupMenuActionHandlerDelegate
// Adds the current page to the reading list.
- (void)readPageLater;
// Records open settings metric per profile type.
- (void)recordSettingsMetricsPerProfile;
// Records open downloads metric per profile type.
- (void)recordDownloadsMetricsPerProfile;
// Toggles the follow status of the on browsing website. Called when the follow
// menu option has been tapped. Follows or unfollows the website according to
// the current follow status of the website.
- (void)toggleFollowed;
// The current web state.
- (web::WebState*)currentWebState;
@end

#endif  // IOS_CHROME_BROWSER_UI_POPUP_MENU_POPUP_MENU_ACTION_HANDLER_DELEGATE_H_
