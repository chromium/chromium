// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/incognito_web_state_observer.h"

#include <vector>

#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/browser/main/all_web_state_list_observation_registrar.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IncognitoWebStateObserver::IncognitoWebStateObserver() {
  std::vector<ChromeBrowserState*> browser_states =
      GetApplicationContext()
          ->GetChromeBrowserStateManager()
          ->GetLoadedBrowserStates();

  // Observer all incognito browsers' web state lists.
  for (ChromeBrowserState* browser_state : browser_states) {
    DCHECK(!browser_state->IsOffTheRecord());
    registrars_.insert(std::make_unique<AllWebStateListObservationRegistrar>(
        browser_state, std::make_unique<Observer>(this),
        AllWebStateListObservationRegistrar::Mode::INCOGNITO));
  }
}

IncognitoWebStateObserver::~IncognitoWebStateObserver() {}

IncognitoWebStateObserver::Observer::Observer(
    IncognitoWebStateObserver* incognito_tracker)
    : incognito_tracker_(incognito_tracker) {}
IncognitoWebStateObserver::Observer::~Observer() {}

void IncognitoWebStateObserver::Observer::WebStateInsertedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index,
    bool activating) {
  incognito_tracker_->OnIncognitoWebStateAdded();
}

void IncognitoWebStateObserver::Observer::WebStateDetachedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index) {
  incognito_tracker_->OnIncognitoWebStateRemoved();
}

void IncognitoWebStateObserver::Observer::WebStateReplacedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int index) {
  // This is invoked when a Tab is replaced by another Tab without any visible
  // UI change. There is nothing to do since the number of Tabs haven't changed.
}
