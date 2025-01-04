// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_TAB_ORGANIZATION_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_TAB_ORGANIZATION_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_view_controller_protocol.h"

// Menu page representing the "Tab Organization" feature.
@interface AIPrototypingTabOrganizationViewController
    : UIViewController <AIPrototypingViewControllerProtocol>

// Use `initWithFeature` from AIPrototypingViewControllerProtocol instead.
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_TAB_ORGANIZATION_VIEW_CONTROLLER_H_
