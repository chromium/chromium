// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_MINI_MAP_COORDINATOR_MINI_MAP_MEDIATOR_H_
#define IOS_CHROME_BROWSER_MINI_MAP_COORDINATOR_MINI_MAP_MEDIATOR_H_

#import <Foundation/Foundation.h>

#import "ios/chrome/browser/mini_map/coordinator/mini_map_mediator_delegate.h"

class PrefService;

namespace web {
class WebState;
}

// The type of query that is displayed on MiniMap.
// If `kText`, the query is a string containing and address.
// If `kURL`, the query is a link to maps.
enum class MiniMapQueryType { kText, kURL };

// Mediator for the Minimap feature
@interface MiniMapMediator : NSObject

// A delegate to trigger the UI actions of the feature
@property(nonatomic, weak) id<MiniMapMediatorDelegate> delegate;

- (instancetype)initWithPrefs:(PrefService*)prefs
                         type:(MiniMapQueryType)type
                     webState:(web::WebState*)webState
    NS_DESIGNATED_INITIALIZER;
- (instancetype)init NS_UNAVAILABLE;

// Disconnects the mediator. No methods should be called after that.
- (void)disconnect;

// The user triggered a minimap.
- (void)userInitiatedMiniMapWithIPH:(BOOL)showIPH;

// User pressed the content settings from MiniMap screen.
- (void)userOpenedSettingsFromMiniMap;

// User pressed the disable address detection from One tapMiniMap screen.
- (void)userDisabledOneTapSettingFromMiniMap;

// User pressed the disable address detection from URL MiniMap screen.
- (void)userDisabledURLSettingFromMiniMap;

// User pressed the done button in disable confirmation snackbar.
- (void)userOpenedSettingsFromDisableConfirmation;

// User pressed the "Report an issue" button from MiniMap screen.
- (void)userReportedAnIssueFromMiniMap;

// User closed the MiniMap.
- (void)userClosedMiniMap;

// User opened a URL from the MiniMap.
- (void)userOpenedURLFromMiniMap;

// User opened a query from the MiniMap.
- (void)userOpenedQueryFromMiniMap;

@end

#endif  // IOS_CHROME_BROWSER_MINI_MAP_COORDINATOR_MINI_MAP_MEDIATOR_H_
