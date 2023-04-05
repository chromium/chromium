// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/tabs/inactive_tabs/inactive_tabs_settings_mediator.h"

#import "components/prefs/ios/pref_observer_bridge.h"
#import "components/prefs/pref_change_registrar.h"
#import "components/prefs/pref_service.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/main/browser_list.h"
#import "ios/chrome/browser/main/browser_list_factory.h"
#import "ios/chrome/browser/prefs/pref_names.h"
#import "ios/chrome/browser/tabs/inactive_tabs/features.h"
#import "ios/chrome/browser/tabs/inactive_tabs/utils.h"
#import "ios/chrome/browser/ui/main/scene_state.h"
#import "ios/chrome/browser/ui/main/scene_state_browser_agent.h"
#import "ios/chrome/browser/ui/settings/tabs/inactive_tabs/inactive_tabs_settings_consumer.h"
#import "ios/chrome/browser/ui/settings/tabs/inactive_tabs/inactive_tabs_settings_table_view_controller.h"
#import "ios/chrome/browser/ui/settings/tabs/inactive_tabs/inactive_tabs_settings_table_view_controller_delegate.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Find the associated inactive browser to the current regular browser.
Browser* FindInactiveBrowserWithRegularBrowser(Browser* regular_browser) {
  ChromeBrowserState* browserState = regular_browser->GetBrowserState();
  SceneState* current_scene_state =
      SceneStateBrowserAgent::FromBrowser(regular_browser)->GetSceneState();
  std::set<Browser*> regular_browsers =
      BrowserListFactory::GetForBrowserState(browserState)
          ->AllRegularBrowsers();

  std::set<Browser*>::iterator inactive_browser_iterator =
      base::ranges::find_if(regular_browsers, [current_scene_state](
                                                  Browser* browser) {
        return browser->IsInactive() &&
               SceneStateBrowserAgent::FromBrowser(browser)->GetSceneState() ==
                   current_scene_state;
      });

  DCHECK(inactive_browser_iterator != regular_browsers.end());

  return *inactive_browser_iterator;
}

}  // namespace

@interface InactiveTabsSettingsMediator () <PrefObserverDelegate>
@end

@implementation InactiveTabsSettingsMediator {
  // Preference service from the application context.
  PrefService* _prefs;
  // The consumer that will be notified when the data change.
  __weak id<InactiveTabsSettingsConsumer> _consumer;
  // Pref observer to track changes to prefs.
  std::unique_ptr<PrefObserverBridge> _prefObserverBridge;
  // Registrar for pref changes notifications.
  PrefChangeRegistrar _prefChangeRegistrar;
  // Regular browser.
  Browser* _browser;
}

- (instancetype)initWithUserLocalPrefService:(PrefService*)localPrefService
                                     browser:(Browser*)browser
                                    consumer:(id<InactiveTabsSettingsConsumer>)
                                                 consumer {
  self = [super init];
  if (self) {
    DCHECK(localPrefService);
    DCHECK(consumer);
    DCHECK(browser);
    _prefs = localPrefService;
    _consumer = consumer;
    _browser = browser;
    _prefChangeRegistrar.Init(_prefs);
    _prefObserverBridge.reset(new PrefObserverBridge(self));
    // Register to observe any changes on pref backed values displayed by the
    // screen.
    _prefObserverBridge->ObserveChangesForPreference(
        prefs::kInactiveTabsTimeThreshold, &_prefChangeRegistrar);

    // Use InactiveTabsTimeThreshold() instead of reading the pref value
    // directly as this function also manage flag and default value.
    int currentThreshold = IsInactiveTabsExplictlyDisabledByUser()
                               ? kInactiveTabsDisabledByUser
                               : InactiveTabsTimeThreshold().InDays();
    [_consumer updateCheckedStateWithDaysThreshold:currentThreshold];
  }
  return self;
}

- (void)disconnect {
  _prefChangeRegistrar.RemoveAll();
  _prefObserverBridge.reset();
  _prefs = nil;
  _consumer = nil;
  _browser = nil;
}

#pragma mark - InactiveTabsSettingsTableViewControllerDelegate

- (void)inactiveTabsSettingsTableViewController:
            (InactiveTabsSettingsTableViewController*)
                inactiveTabsSettingsTableViewController
                 didSelectInactiveDaysThreshold:(int)threshold {
  int previousThreshold = _prefs->GetInteger(prefs::kInactiveTabsTimeThreshold);
  if (previousThreshold == threshold) {
    return;
  }
  _prefs->SetInteger(prefs::kInactiveTabsTimeThreshold, threshold);

  Browser* inactiveBrowser = FindInactiveBrowserWithRegularBrowser(_browser);

  if (threshold == kInactiveTabsDisabledByUser) {
    RestoreAllInactiveTabs(inactiveBrowser, _browser);
  } else if (previousThreshold == kInactiveTabsDisabledByUser ||
             previousThreshold > threshold) {
    MoveTabsFromActiveToInactive(_browser, inactiveBrowser);
  } else {
    MoveTabsFromInactiveToActive(inactiveBrowser, _browser);
  }
}

#pragma mark - PrefObserverDelegate

- (void)onPreferenceChanged:(const std::string&)preferenceName {
  if (preferenceName == prefs::kInactiveTabsTimeThreshold) {
    [_consumer updateCheckedStateWithDaysThreshold:
                   _prefs->GetInteger(prefs::kInactiveTabsTimeThreshold)];
  }
}

@end
