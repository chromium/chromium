// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_BANNER_PROMO_MODEL_DEFAULT_BROWSER_BANNER_PROMO_APP_AGENT_H_
#define IOS_CHROME_BROWSER_BANNER_PROMO_MODEL_DEFAULT_BROWSER_BANNER_PROMO_APP_AGENT_H_

#import "ios/chrome/app/application_delegate/observing_app_state_agent.h"

@class DefaultBrowserBannerPromoAppAgent;

// Protocol for observers that want to know when the default browser banner
// promo state changes.
@protocol DefaultBrowserBannerAppAgentObserver <NSObject>

@optional

// Called when the observers should display the promo.
- (void)displayPromoFromAppAgent:(DefaultBrowserBannerPromoAppAgent*)appAgent;

// Called when the observers should hide the promo.
- (void)hidePromoFromAppAgent:(DefaultBrowserBannerPromoAppAgent*)appAgent;

@end

// App agent to manage the Default Browser Banner Promo. It observes navigation
// events in all active web states to determine when to show and hide the promo.
@interface DefaultBrowserBannerPromoAppAgent : SceneObservingAppAgent

// Whether the promo is currently shown.
@property(nonatomic, assign) BOOL promoCurrentlyShown;

// Whether the current UI state allows for a promo to be shown. Setting this to
// `NO` will temporarily pause an active promo session.
@property(nonatomic, assign) BOOL UICurrentlySupportsPromo;

// Observation methods:
- (void)addObserver:(id<DefaultBrowserBannerAppAgentObserver>)observer;
- (void)removeObserver:(id<DefaultBrowserBannerAppAgentObserver>)observer;

// Alerts the app agent that the promo was tapped.
- (void)promoTapped;

// Alerts the app agent that the promo's close button was tapped.
- (void)promoCloseButtonTapped;

// Updates the promo state based on the current active URLs.
- (void)updatePromoState;

// Alerts the app agent that the given scene was disconnected.
- (void)onSceneDisconnected:(SceneState*)sceneState;

@end

#endif  // IOS_CHROME_BROWSER_BANNER_PROMO_MODEL_DEFAULT_BROWSER_BANNER_PROMO_APP_AGENT_H_
