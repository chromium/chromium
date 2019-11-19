// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/metrics/incognito_web_state_observer.h"

#include <vector>

#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_list.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

IncognitoWebStateObserver::IncognitoWebStateObserver() {
  TabModelList::AddObserver(this);

  // Observe all existing off-the-record TabModels' WebStateLists.
  std::vector<ios::ChromeBrowserState*> browser_states =
      GetApplicationContext()
          ->GetChromeBrowserStateManager()
          ->GetLoadedBrowserStates();

  for (ios::ChromeBrowserState* browser_state : browser_states) {
    DCHECK(!browser_state->IsOffTheRecord());

    if (!browser_state->HasOffTheRecordChromeBrowserState())
      continue;
    ios::ChromeBrowserState* otr_browser_state =
        browser_state->GetOffTheRecordChromeBrowserState();

    NSArray<TabModel*>* tab_models =
        TabModelList::GetTabModelsForChromeBrowserState(otr_browser_state);
    for (TabModel* tab_model in tab_models)
      scoped_observer_.Add([tab_model webStateList]);
  }
}

IncognitoWebStateObserver::~IncognitoWebStateObserver() {
  TabModelList::RemoveObserver(this);
}

void IncognitoWebStateObserver::TabModelRegisteredWithBrowserState(
    TabModel* tab_model,
    ios::ChromeBrowserState* browser_state) {
  if (browser_state->IsOffTheRecord() &&
      !scoped_observer_.IsObserving([tab_model webStateList])) {
    scoped_observer_.Add([tab_model webStateList]);
  }
}

void IncognitoWebStateObserver::TabModelUnregisteredFromBrowserState(
    TabModel* tab_model,
    ios::ChromeBrowserState* browser_state) {
  if (browser_state->IsOffTheRecord()) {
    DCHECK(scoped_observer_.IsObserving([tab_model webStateList]));
    scoped_observer_.Remove([tab_model webStateList]);
  }
}

void IncognitoWebStateObserver::WebStateInsertedAt(WebStateList* web_state_list,
                                                   web::WebState* web_state,
                                                   int index,
                                                   bool activating) {
  OnIncognitoWebStateAdded();
}

void IncognitoWebStateObserver::WebStateDetachedAt(WebStateList* web_state_list,
                                                   web::WebState* web_state,
                                                   int index) {
  OnIncognitoWebStateRemoved();
}

void IncognitoWebStateObserver::WebStateReplacedAt(WebStateList* web_state_list,
                                                   web::WebState* old_web_state,
                                                   web::WebState* new_web_state,
                                                   int index) {
  // This is invoked when a Tab is replaced by another Tab without any visible
  // UI change. There is nothing to do since the number of Tabs haven't changed.
}
