// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_COBALT_COBALT_API_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_COBALT_COBALT_API_H_

#import <Foundation/Foundation.h>

#import "base/ios/block_types.h"
#import "ios/chrome/browser/popup_menu/overflow_menu/public/overflow_menu_constants.h"

class Browser;
@class ChromeCoordinator;
class ProfileIOS;
class TabHelperAttacher;
@class UIViewController;
@class ObservingSceneAgent;

namespace web {
class JavaScriptFeature;
class CobaltController;
}  // namespace web

// Completion block for Cobalt alerts, called with `granted` set to true if the
// user accepted the action, or false otherwise.
typedef void (^CobaltAlertCompletion)(bool granted);

// Completion block for Cobalt popups, called when the popup is fully presented.
typedef void (^CobaltPopupCompletion)(NSError* error);

namespace ios::provider {

// Attaches the Cobalt tab helpers using the given `attacher`.
void AttachCobaltTabHelpers(TabHelperAttacher& attacher);

// Attaches the Cobalt browser agents to the given `browser`.
void AttachCobaltBrowserAgentsForActiveBrowser(Browser* browser);

// Ensures the Cobalt profile keyed service factories are built.
void EnsureCobaltProfileKeyedServiceFactoriesBuilt();

// Parameters which can be used to create an overflow menu destination.
struct OverflowMenuDestinationParameters {
  int destination_name_id;
  overflow_menu::Destination destination;
  NSString* symbol_name;
  BOOL system_symbol;
  NSString* accessibility_id;
};

// Returns the parameters for the Cobalt overflow menu destination.
OverflowMenuDestinationParameters GetCobaltOverflowMenuDestinationParameters();

// Returns the coordinator for Cobalt.
ChromeCoordinator* CreateCobaltCoordinator(
    UIViewController* base_view_controller,
    Browser* browser);

// Returns the Cobalt JavaScript feature for `profile`.
web::JavaScriptFeature* GetCobaltJavascriptFeatureForProfile(
    ProfileIOS* profile);

// Returns the Cobalt controller for `profile`.
web::CobaltController* GetCobaltController(ProfileIOS* profile);

// Returns the coordinator for Cobalt alerts.
ChromeCoordinator* CreateCobaltAlertCoordinator(
    UIViewController* base_view_controller,
    Browser* browser,
    NSString* title,
    NSString* message,
    CobaltAlertCompletion completion);

// Returns the coordinator for Cobalt popups.
ChromeCoordinator* CreateCobaltPopupCoordinator(
    UIViewController* base_view_controller,
    Browser* browser,
    UIViewController* popup_view_controller,
    CobaltPopupCompletion completion);

// Returns the Cobalt scene agent.
ObservingSceneAgent* CreateCobaltSceneAgent();

}  // namespace ios::provider

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_COBALT_COBALT_API_H_
