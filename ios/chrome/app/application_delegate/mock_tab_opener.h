// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_MOCK_TAB_OPENER_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_MOCK_TAB_OPENER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/app/application_delegate/tab_opening.h"

struct UrlLoadParams;

// Mocks a class adopting the TabOpening protocol. It saves the arguments of
// -dismissModalsAndMaybeOpenSelectedTabInMode:withUrlLoadParams:dismissOmnibox:
//  completion:. Can also save the arguments of
// -dismissModalsAndOpenMultipleTabsInMode:URLs:dismissOmnibox:completion:.
// This mock assumes the Incognito interstitial setting is disabled, so it
// falls back to `NORMAL` mode if `targetMode` is `UNDETERMINED`.
@interface MockTabOpener : NSObject<TabOpening>
// Arguments for
// -dismissModalsAndMaybeOpenSelectedTabInMode:withUrlLoadParams:dismissOmnibox:
//  completion:.
@property(nonatomic, readonly) UrlLoadParams urlLoadParams;
@property(nonatomic, readonly) ApplicationModeForTabOpening applicationMode;
@property(nonatomic, strong, readonly) void (^completionBlock)(void);
// Argument for
// -dismissModalsAndOpenMultipleTabsInMode:URLs:dismissOmnibox:completion:.
@property(nonatomic, readonly) const std::vector<GURL>& URLs;

// Clear the URL.
- (void)resetURL;

- (ProceduralBlock)completionBlockForTriggeringAction:
    (TabOpeningPostOpeningAction)action;
@end

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_MOCK_TAB_OPENER_H_
