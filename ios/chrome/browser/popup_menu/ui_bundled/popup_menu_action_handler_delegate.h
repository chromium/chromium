// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_POPUP_MENU_UI_BUNDLED_POPUP_MENU_ACTION_HANDLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_POPUP_MENU_UI_BUNDLED_POPUP_MENU_ACTION_HANDLER_DELEGATE_H_

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
// The current web state.
- (web::WebState*)currentWebState;
@end

#endif  // IOS_CHROME_BROWSER_POPUP_MENU_UI_BUNDLED_POPUP_MENU_ACTION_HANDLER_DELEGATE_H_
