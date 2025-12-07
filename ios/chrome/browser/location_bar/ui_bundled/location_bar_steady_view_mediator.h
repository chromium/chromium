// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_STEADY_VIEW_MEDIATOR_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_STEADY_VIEW_MEDIATOR_H_

#import <UIKit/UIKit.h>

class LocationBarModel;
@protocol LocationBarSteadyViewConsumer;
class OverlayPresenter;
class WebStateList;

namespace feature_engagement {
class Tracker;
}

// A mediator object that updates state relating to the LocationBarSteadyView.
// Mostly, this is any property that involves the WebState.
@interface LocationBarSteadyViewMediator : NSObject

- (instancetype)initWithLocationBarModel:(LocationBarModel*)locationBarModel
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// The WebStateList that this mediator listens for any changes on the active web
// state.
@property(nonatomic, assign) WebStateList* webStateList;

// The overlay presenter for OverlayModality::kWebContentArea.
@property(nonatomic, assign) OverlayPresenter* webContentAreaOverlayPresenter;

// The consumer for this object. This can change during the lifetime of this
// object and may be nil.
@property(nonatomic, weak) id<LocationBarSteadyViewConsumer> consumer;

// Feature engagement tracker.
@property(nonatomic, assign) feature_engagement::Tracker* tracker;

// Stops observing all objects.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_STEADY_VIEW_MEDIATOR_H_
