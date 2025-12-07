// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_PAGE_STATE_CHANGE_HANDLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_PAGE_STATE_CHANGE_HANDLER_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "ios/chrome/browser/intelligence/bwg/model/bwg_page_state_change_delegate.h"

class PrefService;

@interface BWGPageStateChangeHandler : NSObject <BWGPageStateChangeDelegate>

- (instancetype)initWithPrefService:(PrefService*)prefService
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Sets the base view controller that BWG is currently presented on.
- (void)setBaseViewController:(UIViewController*)baseViewController;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_BWG_MODEL_BWG_PAGE_STATE_CHANGE_HANDLER_H_
