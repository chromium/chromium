// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GROUP_UTILS_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GROUP_UTILS_H_

#import <Foundation/Foundation.h>

class FaviconLoader;
@class GroupTabInfo;
namespace web {
class WebState;
}

@interface TabGroupUtils : NSObject

// Retrieves GroupTabInfo from the given `webState`.
// `faviconLoader`: used to fetch favicons on Google server, can be `nullptr`.
// `completion`: the block is executed with the fetched GroupTabInfo.
+ (void)fetchTabGroupInfoFromWebState:(web::WebState*)webState
                        faviconLoader:(FaviconLoader*)faviconLoader
                           completion:(void (^)(GroupTabInfo*))completion;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GROUP_UTILS_H_
