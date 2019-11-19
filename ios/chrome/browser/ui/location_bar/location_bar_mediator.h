// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_MEDIATOR_H_

#import <UIKit/UIKit.h>

@protocol LocationBarConsumer;
class TemplateURLService;
class WebStateList;
class OverlayPresenter;
class LocationBarModel;

// A mediator object that updates the mediator when the web state changes.
@interface LocationBarMediator : NSObject

- (instancetype)initWithLocationBarModel:(LocationBarModel*)locationBarModel
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// The WebStateList that this mediator listens for any changes on the active web
// state.
@property(nonatomic, assign) WebStateList* webStateList;

// The overlay presenter for OverlayModality::kWebContentArea.  This mediator
// listens for overlay presentation events to determine whether the share button
// should be enabled.
@property(nonatomic, assign) OverlayPresenter* webContentAreaOverlayPresenter;

// The location bar model used by this mediator to extract the current URL and
// the security state.
@property(nonatomic, assign, readonly) LocationBarModel* locationBarModel;

// The templateURLService used by this mediator to extract whether the default
// search engine supports search-by-image.
@property(nonatomic, assign) TemplateURLService* templateURLService;

// The consumer for this object. This can change during the lifetime of this
// object and may be nil.
@property(nonatomic, weak) id<LocationBarConsumer> consumer;

// Stops observing all objects.
- (void)disconnect;

@end

#endif  // IOS_CHROME_BROWSER_UI_LOCATION_BAR_LOCATION_BAR_MEDIATOR_H_
