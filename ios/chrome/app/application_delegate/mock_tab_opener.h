// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_APPLICATION_DELEGATE_MOCK_TAB_OPENER_H_
#define IOS_CHROME_APP_APPLICATION_DELEGATE_MOCK_TAB_OPENER_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/app/application_delegate/tab_opening.h"

struct UrlLoadParams;

// Mocks a class adopting the TabOpening protocol. It saves the arguments of
// -dismissModalsAndOpenSelectedTabInMode:withUrlLoadParams:dismissOmnibox:
//  completion:.
@interface MockTabOpener : NSObject<TabOpening>
// Arguments for
// -dismissModalsAndOpenSelectedTabInMode:withUrlLoadParams:dismissOmnibox:
//  completion:.
@property(nonatomic, readonly) UrlLoadParams urlLoadParams;
@property(nonatomic, readonly) ApplicationModeForTabOpening applicationMode;
@property(nonatomic, strong, readonly) void (^completionBlock)(void);

// Clear the URL.
- (void)resetURL;

- (ProceduralBlock)completionBlockForTriggeringAction:
    (NTPTabOpeningPostOpeningAction)action;
@end

#endif  // IOS_CHROME_APP_APPLICATION_DELEGATE_MOCK_TAB_OPENER_H_
