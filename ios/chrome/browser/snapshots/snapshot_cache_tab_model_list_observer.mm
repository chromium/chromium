// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/snapshot_cache_tab_model_list_observer.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_list.h"
#import "ios/chrome/browser/tabs/tab_model_list_observer.h"

SnapshotCacheTabModelListObserver::SnapshotCacheTabModelListObserver(
    ios::ChromeBrowserState* browser_state,
    std::unique_ptr<WebStateListObserver> web_state_list_observer)
    : browser_state_(browser_state),
      web_state_list_observer_(std::move(web_state_list_observer)),
      scoped_observer_(
          std::make_unique<ScopedObserver<WebStateList, WebStateListObserver>>(
              web_state_list_observer_.get())) {
  TabModelList::AddObserver(this);

  // Register as an observer for all TabModels for both the normal and otr
  // browser states.
  DCHECK(!browser_state_->IsOffTheRecord());
  for (TabModel* model :
       TabModelList::GetTabModelsForChromeBrowserState(browser_state_)) {
    scoped_observer_->Add(model.webStateList);
  }

  if (browser_state_->HasOffTheRecordChromeBrowserState()) {
    ios::ChromeBrowserState* otr_state =
        browser_state->GetOffTheRecordChromeBrowserState();
    for (TabModel* model :
         TabModelList::GetTabModelsForChromeBrowserState(otr_state)) {
      scoped_observer_->Add(model.webStateList);
    }
  }
}

SnapshotCacheTabModelListObserver::~SnapshotCacheTabModelListObserver() {
  TabModelList::RemoveObserver(this);
}

void SnapshotCacheTabModelListObserver::TabModelRegisteredWithBrowserState(
    TabModel* tab_model,
    ios::ChromeBrowserState* browser_state) {
  // Normal and Incognito browser states share a SnapshotCache.
  if (browser_state_ == browser_state->GetOriginalChromeBrowserState()) {
    scoped_observer_->Add(tab_model.webStateList);
  }
}

void SnapshotCacheTabModelListObserver::TabModelUnregisteredFromBrowserState(
    TabModel* tab_model,
    ios::ChromeBrowserState* browser_state) {
  // Normal and Incognito browser states share a SnapshotCache.
  if (browser_state_ == browser_state->GetOriginalChromeBrowserState()) {
    scoped_observer_->Remove(tab_model.webStateList);
  }
}
