// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MINI_MAP_UI_BUNDLED_MINI_MAP_MEDIATOR_H_
#define IOS_CHROME_BROWSER_MINI_MAP_UI_BUNDLED_MINI_MAP_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/mini_map/ui_bundled/mini_map_mediator_delegate.h"

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

// User dismissed the consent window.
- (void)userDismissed;

// User pressed the content settings from consent screen.
- (void)userOpenedSettingsInConsent;

// User pressed the content settings from MiniMap screen.
- (void)userOpenedSettingsFromMiniMap;

// User pressed the "Report an issue" button from MiniMap screen.
- (void)userReportedAnIssueFromMiniMap;

// User closed the MiniMap.
- (void)userClosedMiniMap;

// User opened a URL from the MiniMap.
- (void)userOpenedURLFromMiniMap;

@end

#endif  // IOS_CHROME_BROWSER_MINI_MAP_UI_BUNDLED_MINI_MAP_MEDIATOR_H_
