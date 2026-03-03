// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_APC_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_APC_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_view_controller_protocol.h"

// Page to extract and view the Annotated Page Content(APC) of the current page.
@interface AIPrototypingAPCViewController
    : UIViewController <AIPrototypingViewControllerProtocol>

// Use `initWithFeature` from AIPrototypingViewControllerProtocol instead.
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_APC_VIEW_CONTROLLER_H_
