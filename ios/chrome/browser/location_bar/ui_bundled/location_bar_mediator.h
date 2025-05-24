// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_MEDIATOR_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_MEDIATOR_H_

#import <UIKit/UIKit.h>

@protocol LocationBarConsumer;
class TemplateURLService;
class WebStateList;

// A mediator object that updates location bar state not relating to the
// LocationBarSteadyView. In practice, this is any state not relating to the
// WebState.
@interface LocationBarMediator : NSObject

- (instancetype)initWithIsIncognito:(BOOL)isIncognito NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;
;

// The templateURLService used by this mediator to extract whether the default
// search engine supports search-by-image.
@property(nonatomic, assign) TemplateURLService* templateURLService;

// The consumer for this object. This can change during the lifetime of this
// object and may be nil.
@property(nonatomic, weak) id<LocationBarConsumer> consumer;

// The WebStateList that this mediator listens for any changes on the active web
// state.
@property(nonatomic, assign) WebStateList* webStateList;

// Stops observing all objects.
- (void)disconnect;

// Called when the location is updated.
- (void)locationUpdated;

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_MEDIATOR_H_
