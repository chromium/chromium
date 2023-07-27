// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_SPOTLIGHT_ACTIONS_SPOTLIGHT_MANAGER_H_
#define IOS_CHROME_APP_SPOTLIGHT_ACTIONS_SPOTLIGHT_MANAGER_H_

#import <Foundation/Foundation.h>

@class AppStartupParameters;
@class SpotlightInterface;
@class SearchableItemFactory;

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
@interface ActionsSpotlightManager : NSObject

// Creates an ActionsSpotlightManager.
+ (ActionsSpotlightManager*)actionsSpotlightManager;

- (instancetype)
    initWithSpotlightInterface:(SpotlightInterface*)spotlightInterface
         searchableItemFactory:(SearchableItemFactory*)searchableItemFactory;

- (instancetype)init NS_UNAVAILABLE;

/// Facade interface for the spotlight API.
@property(nonatomic, readonly) SpotlightInterface* spotlightInterface;

/// A searchable item factory to create searchable items.
@property(nonatomic, readonly) SearchableItemFactory* searchableItemFactory;

// Updates the index with the Spotlight actions if the EnableSpotlightActions
// experimental flag is set. Otherwise the index is only cleared.
- (void)indexActionsWithIsGoogleDefaultSearchEngine:
    (BOOL)isGoogleDefaultSearchEngine;

// Called before the instance is deallocated.
- (void)shutdown NS_REQUIRES_SUPER;

@end

#endif  // IOS_CHROME_APP_SPOTLIGHT_ACTIONS_SPOTLIGHT_MANAGER_H_
