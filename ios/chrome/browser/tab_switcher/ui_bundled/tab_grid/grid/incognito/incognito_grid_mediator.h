// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_INCOGNITO_INCOGNITO_GRID_MEDIATOR_H_
#define IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_INCOGNITO_INCOGNITO_GRID_MEDIATOR_H_

#import "ios/chrome/browser/tab_switcher/tab_grid/base_grid/coordinator/base_grid_mediator.h"

namespace feature_engagement {
class Tracker;
}
@protocol IncognitoGridMediatorDelegate;
@class IncognitoReauthSceneAgent;
@protocol TabGroupsCommands;

namespace signin {
class IdentityManager;
}

// Mediates between model layer and incognito grid UI layer.
@interface IncognitoGridMediator : BaseGridMediator

// Incognito mediator delegate.
@property(nonatomic, weak) id<IncognitoGridMediatorDelegate> incognitoDelegate;
// The reauth scene agent to handle the button enabled state.
@property(nonatomic, weak) IncognitoReauthSceneAgent* reauthSceneAgent;
// The feature engagement tracker to alert of promo events.
@property(nonatomic, assign) feature_engagement::Tracker* tracker;

// Initializes the capabilities observer to track changes to Family Link state.
- (void)initializeFamilyLinkUserCapabilitiesObserver:
    (signin::IdentityManager*)identityManager;

@end

#endif  // IOS_CHROME_BROWSER_TAB_SWITCHER_UI_BUNDLED_TAB_GRID_GRID_INCOGNITO_INCOGNITO_GRID_MEDIATOR_H_
