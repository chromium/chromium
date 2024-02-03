// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_HELPER_H_
#define IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_HELPER_H_

#import <UIKit/UIKit.h>

namespace page_info {

// Returns an `UILabel` for the navigationItem titleView for the given
// `siteURL`.
UILabel* TitleViewLabelForURL(NSString* site_url);

}  // namespace page_info

#endif  // IOS_CHROME_BROWSER_UI_PAGE_INFO_PAGE_INFO_HELPER_H_
