// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_TAB_OPENING_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_TAB_OPENING_H_

#include "base/ios/block_types.h"
#import "ios/chrome/app/app_startup_parameters.h"
#include "ios/chrome/app/application_mode.h"
#include "ui/base/page_transition_types.h"

@class AppState;
class GURL;
@class TabModel;
@protocol StartupInformation;
struct UrlLoadParams;

enum class ApplicationModeForTabOpening { NORMAL, INCOGNITO, CURRENT };

// Protocol for object that can open new tabs during application launch.
@protocol TabOpening<NSObject>

// Dismisses any modal view, excluding the omnibox if |dismissOmnibox| is NO,
// then opens either a normal or incognito tab with |url|. After opening |url|,
// run completion |handler| if it is not nil. After Tab is opened the virtual
// URL is set to the pending navigation item.
- (void)dismissModalsAndOpenSelectedTabInMode:
            (ApplicationModeForTabOpening)targetMode
                            withUrlLoadParams:
                                (const UrlLoadParams&)urlLoadParams
                               dismissOmnibox:(BOOL)dismissOmnibox
                                   completion:(ProceduralBlock)completion;

// Creates a new tab if the launch options are not null.
- (void)openTabFromLaunchOptions:(NSDictionary*)launchOptions
              startupInformation:(id<StartupInformation>)startupInformation
                        appState:(AppState*)appState;

// Returns whether an NTP tab should be opened when the specified tabModel is
// made current.
- (BOOL)shouldOpenNTPTabOnActivationOfTabModel:(TabModel*)tabModel;

// Returns a block that can be executed on the new tab to trigger one of the
// commands. This block can be passed to
// |dismissModalsAndOpenSelectedTabInMode:withURL:transition:completion:|.
// This block must only be executed if new tab opened on NTP.
- (ProceduralBlock)completionBlockForTriggeringAction:
    (NTPTabOpeningPostOpeningAction)action;

// Attempts to complete a Payment Request flow with a payment response from a
// a third party app. Returns whether or not this operation was successful.
- (BOOL)shouldCompletePaymentRequestOnCurrentTab:
    (id<StartupInformation>)startupInformation;

// Whether the |URL| is already opened, in regular mode.
- (BOOL)URLIsOpenedInRegularMode:(const GURL&)URL;

@end

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_TAB_OPENING_H_
