// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_MENU_COORDINATOR_COMPOSEBOX_MENU_SHARED_TAB_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_MENU_COORDINATOR_COMPOSEBOX_MENU_SHARED_TAB_H_

#import <UIKit/UIKit.h>

#import "url/gurl.h"

namespace base {
class UnguessableToken;
}  // namespace base

// Represents a shared tab result.
@interface ComposeboxMenuSharedTab : NSObject

// The URL of the tab.
@property(nonatomic, readonly) GURL URL;
// The title of the tab.
@property(nonatomic, readonly) NSString* title;
// The server token for this tab.
@property(nonatomic, readonly) base::UnguessableToken serverToken;

// The favicon of the web page.
@property(nonatomic, strong, readonly) UIImage* favicon;

- (instancetype)initWithURL:(GURL)URL
                      title:(NSString*)title
                serverToken:(base::UnguessableToken)serverToken
                    favicon:(UIImage*)favicon NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_MENU_COORDINATOR_COMPOSEBOX_MENU_SHARED_TAB_H_
