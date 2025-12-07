// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_VIEW_CONTROLLER_TESTING_H_
#define IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_VIEW_CONTROLLER_TESTING_H_

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_header_view_controller.h"

@class NewTabPageHeaderView;

// Testing category that is intended to only be imported in tests.
@interface NewTabPageHeaderViewController (Testing)

@property(nonatomic, strong, readonly) NewTabPageHeaderView* headerView;

@property(nonatomic, strong, readonly) UIImage* identityDiscImage;

@end

#endif  // IOS_CHROME_BROWSER_NTP_UI_BUNDLED_NEW_TAB_PAGE_HEADER_VIEW_CONTROLLER_TESTING_H_
