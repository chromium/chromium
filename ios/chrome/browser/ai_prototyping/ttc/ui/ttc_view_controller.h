// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_UI_TTC_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_UI_TTC_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_view_controller_protocol.h"

// View controller displaying the "TalkToChrome" (TTC) feature.
@interface TTCViewController
    : UIViewController <AIPrototypingViewControllerProtocol>

// Mutator handling common AI prototyping interactions.
@property(nonatomic, weak) id<AIPrototypingMutator> mutator;

// Use `initForFeature:` from `AIPrototypingViewControllerProtocol` instead.
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_TTC_UI_TTC_VIEW_CONTROLLER_H_
