// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/model/incognito_web_state_observer.h"

#import <vector>

#import "ios/chrome/browser/shared/model/application_context/application_context.h"
#import "ios/chrome/browser/shared/model/browser/all_web_state_list_observation_registrar.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state_manager.h"

IncognitoWebStateObserver::IncognitoWebStateObserver() {
  std::vector<ChromeBrowserState*> browser_states =
      GetApplicationContext()
          ->GetChromeBrowserStateManager()
          ->GetLoadedBrowserStates();

  // Observer all incognito browsers' web state lists.
  for (ChromeBrowserState* browser_state : browser_states) {
    DCHECK(!browser_state->IsOffTheRecord());
    registrars_.insert(std::make_unique<AllWebStateListObservationRegistrar>(
        BrowserListFactory::GetForBrowserState(browser_state),
        std::make_unique<Observer>(this),
        AllWebStateListObservationRegistrar::Mode::INCOGNITO));
  }
}

IncognitoWebStateObserver::~IncognitoWebStateObserver() {}

IncognitoWebStateObserver::Observer::Observer(
    IncognitoWebStateObserver* incognito_tracker)
    : incognito_tracker_(incognito_tracker) {}
IncognitoWebStateObserver::Observer::~Observer() {}

#pragma mark - WebStateListObserver

void IncognitoWebStateObserver::Observer::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // Do nothing when a WebState is selected and its status is updated.
      break;
    case WebStateListChange::Type::kDetach:
      incognito_tracker_->OnIncognitoWebStateRemoved();
      break;
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace:
      // This is invoked when a Tab is replaced by another Tab without any
      // visible UI change. There is nothing to do since the number of Tabs
      // haven't changed.
      break;
    case WebStateListChange::Type::kInsert:
      incognito_tracker_->OnIncognitoWebStateAdded();
      break;
  }
}
