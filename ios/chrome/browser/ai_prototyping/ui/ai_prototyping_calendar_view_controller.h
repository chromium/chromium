// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_CALENDAR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_CALENDAR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_view_controller_protocol.h"

// Enhanced calendar view controller as one page of the AI prototyping menu.
@interface AIPrototypingCalendarViewController
    : UIViewController <AIPrototypingViewControllerProtocol>

// Use `initWithFeature` from AIPrototypingViewControllerProtocol instead.
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_CALENDAR_VIEW_CONTROLLER_H_
