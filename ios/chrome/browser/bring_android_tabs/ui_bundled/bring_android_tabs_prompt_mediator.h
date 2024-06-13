// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_BRING_ANDROID_TABS_PROMPT_MEDIATOR_H_
#define IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_BRING_ANDROID_TABS_PROMPT_MEDIATOR_H_

#import "ios/chrome/browser/bring_android_tabs/ui_bundled/bring_android_tabs_prompt_view_controller_delegate.h"

#import <Foundation/Foundation.h>

class BringAndroidTabsToIOSService;
class UrlLoadingBrowserAgent;

// Mediator for "Bring Android Tabs" prompt that manages model interactions.
@interface BringAndroidTabsPromptMediator
    : NSObject <BringAndroidTabsPromptViewControllerDelegate>

// Designated initializer of the mediator, with `tabs` as a list of active tabs
// on the user's previous Android device to be brought to the current iOS
// device.
- (instancetype)
    initWithBringAndroidTabsService:(BringAndroidTabsToIOSService*)service
                          URLLoader:(UrlLoadingBrowserAgent*)URLLoader
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_BRING_ANDROID_TABS_UI_BUNDLED_BRING_ANDROID_TABS_PROMPT_MEDIATOR_H_
