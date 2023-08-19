// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_MINI_MAP_MINI_MAP_MEDIATOR_H_
#define IOS_CHROME_BROWSER_UI_MINI_MAP_MINI_MAP_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/ui/mini_map/mini_map_mediator_delegate.h"

class PrefService;

namespace web {
class WebState;
}

// Mediator for the Minimap feature
@interface MiniMapMediator : NSObject

// A delegate to trigger the UI actions of the feature
@property(nonatomic, weak) id<MiniMapMediatorDelegate> delegate;

- (instancetype)initWithPrefs:(PrefService*)prefs
                     webState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator. No methods should be called after that.
- (void)disconnect;

// The user triggered a minimap.
- (void)userInitiatedMiniMapConsentRequired:(BOOL)consentRequired;

// User consented in the interstitial.
- (void)userConsented;

// User did not consent in the interstitial.
- (void)userDeclined;

// User dismissed the window.
- (void)userDismissed;

// User pressed the content settings.
- (void)userOpenedSettings;

@end

#endif
