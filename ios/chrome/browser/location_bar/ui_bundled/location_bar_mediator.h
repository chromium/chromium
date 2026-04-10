// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_MEDIATOR_H_
#define IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_MEDIATOR_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/fullscreen/model/fullscreen_browser_agent_observer_bridge.h"
#import "ios/chrome/browser/fullscreen/ui_bundled/fullscreen_ui_element.h"
#import "ios/chrome/browser/location_bar/ui_bundled/location_bar_mutator.h"

@protocol LocationBarConsumer;
class OmniboxPositionBrowserAgent;
class PlaceholderService;
class TemplateURLService;
class UrlLoadingBrowserAgent;
class WebStateList;

// A mediator object that updates location bar state not relating to the
// LocationBarSteadyView. In practice, this is any state not relating to the
// WebState.
@interface LocationBarMediator
    : NSObject <LocationBarMutator, FullscreenBrowserAgentObserving>

- (instancetype)initWithURLLoadingBrowsingAgent:
                    (UrlLoadingBrowserAgent*)URLLoadingBrowserAgent
                                    isIncognito:(BOOL)isIncognito
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// The templateURLService used by this mediator to extract whether the default
// search engine supports search-by-image.
@property(nonatomic, assign) TemplateURLService* templateURLService;

/// The placeholder used by this mediator to extract placeholder text and image.
@property(nonatomic, assign) PlaceholderService* placeholderService;

// The consumer for this object. This can change during the lifetime of this
// object and may be nil.
@property(nonatomic, weak) id<LocationBarConsumer> consumer;

// The WebStateList that this mediator listens for any changes on the active web
// state.
@property(nonatomic, assign) WebStateList* webStateList;

// The OmniboxPositionBrowserAgent used to query the position of the omnibox.
@property(nonatomic, assign)
    OmniboxPositionBrowserAgent* omniboxPositionBrowserAgent;

// Whether the location bar is the active one. Only set when `kChromeNextIa` is
// enabled.
@property(nonatomic, assign) BOOL active;

// Whether the location bar is at the top or bottom position. Only set when
// `kChromeNextIa` is enabled.
@property(nonatomic, assign) BOOL topPosition;

// Adds a fullscreen UI element to be notified of fullscreen progress.
- (void)addFullscreenUIElement:(id<FullscreenUIElement>)element;

// Stops observing all objects.
- (void)disconnect;

// Called when the location is updated.
- (void)locationUpdated;

@end

#endif  // IOS_CHROME_BROWSER_LOCATION_BAR_UI_BUNDLED_LOCATION_BAR_MEDIATOR_H_
