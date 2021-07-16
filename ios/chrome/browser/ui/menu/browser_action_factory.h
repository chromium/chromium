// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MENU_BROWSER_ACTION_FACTORY_H_
#define IOS_CHROME_BROWSER_UI_MENU_BROWSER_ACTION_FACTORY_H_

#import <UIKit/UIKit.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/ui/menu/action_factory.h"
#import "ios/chrome/browser/ui/menu/menu_action_type.h"
#import "ios/chrome/browser/window_activities/window_activity_helpers.h"

class Browser;
class GURL;

// Factory providing methods to create UIActions that depends on the provided
// browser with consistent titles, images and metrics structure.
@interface BrowserActionFactory : ActionFactory

// Initializes a factory instance for the current |browser| to create action
// instances for the given |scenario|. |browser| can be nil, and in that case
// only actions that doesn't require browser will work, and other actions will
// return nil;
- (instancetype)initWithBrowser:(Browser*)browser
                       scenario:(MenuScenario)scenario;

// Creates a UIAction instance configured for opening the |URL| in a new tab and
// which will invoke the given |completion| block after execution.
- (UIAction*)actionToOpenInNewTabWithURL:(const GURL)URL
                              completion:(ProceduralBlock)completion;

// Creates a UIAction instance configured for opening the |URL| in a new
// incognito tab and which will invoke the given |completion| block after
// execution.
- (UIAction*)actionToOpenInNewIncognitoTabWithURL:(const GURL)URL
                                       completion:(ProceduralBlock)completion;

// Creates a UIAction instance whose title and icon are configured for opening a
// URL in a new incognito tab. When triggered, the action will invoke the
// |block| which needs to open a URL in a new incognito tab.
- (UIAction*)actionToOpenInNewIncognitoTabWithBlock:(ProceduralBlock)block;

// Creates a UIAction instance configured for opening the |URL| in a new window
// from |activityOrigin|.
- (UIAction*)actionToOpenInNewWindowWithURL:(const GURL)URL
                             activityOrigin:
                                 (WindowActivityOrigin)activityOrigin;

// Creates a UIAction instance for opening an image |URL| in current tab and
// invoke the given |completion| block after execution.
- (UIAction*)actionOpenImageWithURL:(const GURL)URL
                         completion:(ProceduralBlock)completion;

// Creates a UIAction instance for opening an image |params| in a new tab and
// invoke the given |completion| block after execution.
- (UIAction*)actionOpenImageInNewTabWithUrlLoadParams:(UrlLoadParams)params
                                           completion:
                                               (ProceduralBlock)completion;

@end

#endif  // IOS_CHROME_BROWSER_UI_MENU_BROWSER_ACTION_FACTORY_H_
