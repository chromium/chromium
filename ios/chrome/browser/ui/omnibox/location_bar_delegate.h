// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_LOCATION_BAR_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_LOCATION_BAR_DELEGATE_H_

#import <Foundation/Foundation.h>

class LocationBarModel;

namespace web {
class WebState;
}
// Delegate for LocationBarController objects.  Used to provide the location bar
// a way to open URLs and otherwise interact with the browser.
@protocol LocationBarDelegate
- (void)locationBarHasBecomeFirstResponder;
- (void)locationBarHasResignedFirstResponder;
- (web::WebState*)webState;
- (LocationBarModel*)locationBarModel;
@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_LOCATION_BAR_DELEGATE_H_
