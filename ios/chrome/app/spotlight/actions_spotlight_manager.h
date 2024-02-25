// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_SPOTLIGHT_ACTIONS_SPOTLIGHT_MANAGER_H_
#define IOS_CHROME_APP_SPOTLIGHT_ACTIONS_SPOTLIGHT_MANAGER_H_

#import "ios/chrome/app/spotlight/base_spotlight_manager.h"

@class AppStartupParameters;

namespace spotlight {

// Keys for Spotlight actions.
extern const char kSpotlightActionNewTab[];
extern const char kSpotlightActionNewIncognitoTab[];
extern const char kSpotlightActionVoiceSearch[];
extern const char kSpotlightActionQRScanner[];

// Sets the correct properties for startup parameters according to the action
// specified by the `query`. Returns YES if the properties were successfully
// set. The query must represent an action and `startupParams` must not be nil.
BOOL SetStartupParametersForSpotlightAction(
    NSString* query,
    AppStartupParameters* startupParams);

}  // namespace spotlight

// Allows Chrome to add links to actions to the systemwide Spotlight search
// index.
@interface ActionsSpotlightManager : BaseSpotlightManager

// Creates an ActionsSpotlightManager.
+ (ActionsSpotlightManager*)actionsSpotlightManager;

- (instancetype)
    initWithSpotlightInterface:(SpotlightInterface*)spotlightInterface
         searchableItemFactory:(SearchableItemFactory*)searchableItemFactory;

- (instancetype)init NS_UNAVAILABLE;

// Updates the index with the Spotlight actions if the EnableSpotlightActions
// experimental flag is set. Otherwise the index is only cleared.
- (void)indexActionsWithIsGoogleDefaultSearchEngine:
    (BOOL)isGoogleDefaultSearchEngine;

@end

#endif  // IOS_CHROME_APP_SPOTLIGHT_ACTIONS_SPOTLIGHT_MANAGER_H_
