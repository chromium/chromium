// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_ACTOR_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_ACTOR_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/ai_prototyping/ui/ai_prototyping_view_controller_protocol.h"

// Actor view controller as one page of the AI prototyping menu.
@interface AIPrototypingActorViewController
    : UIViewController <AIPrototypingViewControllerProtocol>

// Use `initWithFeature` from AIPrototypingViewControllerProtocol instead.
- (instancetype)init NS_UNAVAILABLE;

// Updates the list of tabs available for selection.
- (void)updateTabList:(NSArray<NSDictionary*>*)tabs;

// Updates the representation of the current tabs FrameData and ContentNodes, as
// fetched from the AnnotatedPageContent.
- (void)updateFramesAndContentNodesDebugString:(NSString*)debugString;

// Updates the list of frames available for selection.
- (void)updateFrameList:(NSArray<NSDictionary*>*)frames;

@end

#endif  // IOS_CHROME_BROWSER_AI_PROTOTYPING_UI_AI_PROTOTYPING_ACTOR_VIEW_CONTROLLER_H_
