// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONTROLLER_DELEGATE_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONTROLLER_DELEGATE_H_

#import <Foundation/Foundation.h>

class LocationBarModel;

namespace web {
class WebState;
}
// Delegate for omniboxController object. Used to provide the location bar
// a way to open URLs and otherwise interact with the browser.
@protocol OmniboxControllerDelegate
- (web::WebState*)webState;
- (LocationBarModel*)locationBarModel;
@end

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_CONTROLLER_DELEGATE_H_
