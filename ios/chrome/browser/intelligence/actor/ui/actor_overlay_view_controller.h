// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTOR_OVERLAY_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTOR_OVERLAY_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

@class SceneLayoutState;

@class LayoutGuideCenter;

// A view controller managing the actor overlay view.
@interface ActorOverlayViewController : UIViewController

// The layout state observed by this view controller.
@property(nonatomic, weak) SceneLayoutState* layoutState;

// Initializes the view controller using `browserCenter` to anchor subviews to
// browser layout guides, `scrimColor` as the base scrim color, and `glowColor`
// as the base glow color.
- (instancetype)initWithBrowserLayoutGuideCenter:
                    (LayoutGuideCenter*)browserCenter
                                      scrimColor:(UIColor*)scrimColor
                                       glowColor:(UIColor*)glowColor
    NS_DESIGNATED_INITIALIZER;

- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)coder NS_UNAVAILABLE;
- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_INTELLIGENCE_ACTOR_UI_ACTOR_OVERLAY_VIEW_CONTROLLER_H_
