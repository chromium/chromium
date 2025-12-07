// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_INCOGNITO_BADGE_VIEW_CONTROLLER_H_
#define IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_INCOGNITO_BADGE_VIEW_CONTROLLER_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/badges/ui_bundled/incognito_badge_consumer.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_element.h"

@class BadgeButtonFactory;
@protocol IncognitoBadgeViewVisibilityDelegate;

// Manages the display of the incognito badge.
@interface IncognitoBadgeViewController
    : UIViewController <FullscreenUIElement, IncognitoBadgeConsumer>

// The badge view visibility delegate.
@property(nonatomic, weak) id<IncognitoBadgeViewVisibilityDelegate>
    visibilityDelegate;

// `buttonFactory` must be non-nil.
- (instancetype)initWithButtonFactory:(BadgeButtonFactory*)buttonFactory
    NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithNibName:(NSString*)nibNameOrNil
                         bundle:(NSBundle*)nibBundleOrNil NS_UNAVAILABLE;
- (instancetype)initWithCoder:(NSCoder*)aDecoder NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_BADGES_UI_BUNDLED_INCOGNITO_BADGE_VIEW_CONTROLLER_H_
