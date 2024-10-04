// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_TAB_OPENING_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_TAB_OPENING_H_

#include "base/ios/block_types.h"
#import "ios/chrome/app/app_startup_parameters.h"
#include "ios/chrome/app/application_mode.h"
#include "ui/base/page_transition_types.h"

class Browser;
class GURL;
@protocol StartupInformation;
struct UrlLoadParams;
@class URLOpenerParams;

// Protocol for object that can open new tabs during application launch.
@protocol TabOpening<NSObject>

// 1. Dismisses any modal view, excluding the omnibox if `dismissOmnibox` is NO,
// 2. (only if `targetMode` is UNDETERMINED) Resolves the value of `targetMode`,
//    potentially by presenting the Incognito interstitial to the user and
//    letting them choose, alternatively by falling back to normal mode if the
//    user did not enable the "Ask to Open Links from Other Apps in Incognito"
//    setting.
// 3. Opens either a normal or incognito tab with `urlLoadParams`,
//    unless it was manually cancelled by the user at step 2.
//
// If `completion` is not nil, it is either called once Incognito interstitial
// has been presented, or once a new tab has been opened.
// After Tab is opened the virtual URL is set to the pending navigation item.
- (void)dismissModalsAndMaybeOpenSelectedTabInMode:
            (ApplicationModeForTabOpening)targetMode
                                 withUrlLoadParams:
                                     (const UrlLoadParams&)urlLoadParams
                                    dismissOmnibox:(BOOL)dismissOmnibox
                                        completion:(ProceduralBlock)completion;

// Dismisses any modal view, excluding the omnibox if `dismissOmnibox` is NO,
// then opens the list of URLs in `URLs` in either normal or incognito.
// After opening the array of URLs, run completion `handler` if it not nil.
- (void)dismissModalsAndOpenMultipleTabsWithURLs:(const std::vector<GURL>&)URLs
                                 inIncognitoMode:(BOOL)incognitoMode
                                  dismissOmnibox:(BOOL)dismissOmnibox
                                      completion:(ProceduralBlock)completion;

// Creates a new tab if the launch options are not null.
- (void)openTabFromLaunchWithParams:(URLOpenerParams*)params
                 startupInformation:(id<StartupInformation>)startupInformation;

// Returns whether an NTP tab should be opened when the specified browser is
// made current.
- (BOOL)shouldOpenNTPTabOnActivationOfBrowser:(Browser*)browser;

// Returns a block that can be executed on the new tab to trigger one of the
// commands. This block can be passed to
// `dismissModalsAndMaybeOpenSelectedTabInMode:withURL:transition:completion:`.
// This block must only be executed if new tab opened on NTP.
- (ProceduralBlock)completionBlockForTriggeringAction:
    (TabOpeningPostOpeningAction)action;

// Whether the `URL` is already opened, in regular mode.
- (BOOL)URLIsOpenedInRegularMode:(const GURL&)URL;

@end

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_TAB_OPENING_H_
