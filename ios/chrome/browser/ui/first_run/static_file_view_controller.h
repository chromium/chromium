// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_FIRST_RUN_STATIC_FILE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_UI_FIRST_RUN_STATIC_FILE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

class ChromeBrowserState;

// View controller used to display a bundled file in a web view with a shadow
// below the navigation bar when the user scrolls.
@interface StaticFileViewController : UIViewController

// Initializes with the given URL to display and browser state. Neither
// |browserState| nor |URL| may be nil.
- (instancetype)initWithBrowserState:(ChromeBrowserState*)browserState
                                 URL:(NSURL*)URL;

@end

#endif  // IOS_CHROME_BROWSER_UI_FIRST_RUN_STATIC_FILE_VIEW_CONTROLLER_H_
