// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_APP_SPOTLIGHT_ACTIONS_SPOTLIGHT_MANAGER_H_
#define IOS_CHROME_APP_SPOTLIGHT_ACTIONS_SPOTLIGHT_MANAGER_H_

#import "ios/chrome/app/spotlight/base_spotlight_manager.h"

@class AppStartupParameters;

namespace spotlight {

// Keys for Spotlight actions.
extern NSString* const kSpotlightActionNewTab;
extern NSString* const kSpotlightActionNewIncognitoTab;
extern NSString* const kSpotlightActionVoiceSearch;
extern NSString* const kSpotlightActionQRScanner;
extern NSString* const kSpotlightActionSetDefaultBrowser;
extern NSString* const kSpotlightActionLens;

// Enum used to record the actions performed by the user.
enum {
  // Recorded when a user pressed the New Tab spotlight action.
  SPOTLIGHT_ACTION_NEW_TAB_PRESSED,
  // Recorded when a user pressed the New Incognito Tab spotlight action.
  SPOTLIGHT_ACTION_NEW_INCOGNITO_TAB_PRESSED,
  // Recorded when a user pressed the Voice Search spotlight action.
  SPOTLIGHT_ACTION_VOICE_SEARCH_PRESSED,
  // Recorded when a user pressed the QR scanner spotlight action.
  SPOTLIGHT_ACTION_QR_CODE_SCANNER_PRESSED,
  // Recorded when a user pressed the Set Default Browser spotlight action.
  SPOTLIGHT_ACTION_SET_DEFAULT_BROWSER_PRESSED,
  // Recorded when a user pressed the Lens spotlight action.
  SPOTLIGHT_ACTION_LENS_PRESSED,
  // NOTE: Add new spotlight actions in sources only immediately above this
  // line. Also, make sure the enum list for histogram `SpotlightActions` in
  // histograms.xml is updated with any change in here.
  SPOTLIGHT_ACTION_COUNT
};

// The histogram used to record user actions performed on the spotlight actions.
extern const char kSpotlightActionsHistogram[];

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
