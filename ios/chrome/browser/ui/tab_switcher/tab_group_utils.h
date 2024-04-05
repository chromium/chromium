// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GROUP_UTILS_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GROUP_UTILS_H_

#import <Foundation/Foundation.h>

@class GroupTabInfo;
namespace web {
class WebState;
}

@interface TabGroupUtils : NSObject

// Retrieves GroupTabInfo from the given `webState`.
+ (void)fetchTabGroupInfoFromWebState:(web::WebState*)webState
                           completion:(void (^)(GroupTabInfo*))completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GROUP_UTILS_H_
