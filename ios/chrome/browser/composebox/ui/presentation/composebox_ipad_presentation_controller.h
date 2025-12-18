// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_COMPOSEBOX_UI_PRESENTATION_COMPOSEBOX_IPAD_PRESENTATION_CONTROLLER_H_
#define IOS_CHROME_BROWSER_COMPOSEBOX_UI_PRESENTATION_COMPOSEBOX_IPAD_PRESENTATION_CONTROLLER_H_

#import <UIKit/UIKit.h>

@protocol BrowserCoordinatorCommands;
@class LayoutGuideCenter;

// Presentation controller handling the presentation of the composebox on iPad.
@interface ComposeboxiPadPresentationController : UIPresentationController

// Command handler to dismiss the view.
@property(nonatomic, weak) id<BrowserCoordinatorCommands>
    browserCoordinatorHandler;

// The layout guide center to use for the presentation.
@property(nonatomic, strong) LayoutGuideCenter* layoutGuideCenter;

@end

#endif  // IOS_CHROME_BROWSER_COMPOSEBOX_UI_PRESENTATION_COMPOSEBOX_IPAD_PRESENTATION_CONTROLLER_H_
