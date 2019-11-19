// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_CACHE_TAB_MODEL_LIST_OBSERVER_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_CACHE_TAB_MODEL_LIST_OBSERVER_H_

#include <memory>

#include "base/macros.h"
#include "base/scoped_observer.h"
#import "ios/chrome/browser/tabs/tab_model_list_observer.h"
#include "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/chrome/browser/web_state_list/web_state_list_observer.h"

@class TabModel;

namespace ios {
class ChromeBrowserState;
}

// SnapshotCacheTabModelListObserver tracks when TabModels are created and
// destroyed for a given ChromeBrowserState.  Whenever the TabModelList changes,
// SnapshotCacheTabModelListObserver registers a provided observer as a
// WebStateListObserver.
class SnapshotCacheTabModelListObserver : public TabModelListObserver {
 public:
  // Constructs an object that registers the given |web_state_list_observer| as
  // a WebStateListObserver for any TabModels associated with |browser_state| or
  // |browser_state|'s OTR browser state.  Keeps observer registration up to
  // date as TabModels are created and destroyed. |browser_state| must be a
  // normal (non-OTR) browser state.
  SnapshotCacheTabModelListObserver(
      ios::ChromeBrowserState* browser_state,
      std::unique_ptr<WebStateListObserver> web_state_list_observer);

  ~SnapshotCacheTabModelListObserver() override;

  // TabModelListObserver.
  void TabModelRegisteredWithBrowserState(
      TabModel* tab_model,
      ios::ChromeBrowserState* browser_state) override;
  void TabModelUnregisteredFromBrowserState(
      TabModel* tab_model,
      ios::ChromeBrowserState* browser_state) override;

 private:
  ios::ChromeBrowserState* browser_state_;
  std::unique_ptr<WebStateListObserver> web_state_list_observer_;
  std::unique_ptr<ScopedObserver<WebStateList, WebStateListObserver>>
      scoped_observer_;

  DISALLOW_COPY_AND_ASSIGN(SnapshotCacheTabModelListObserver);
};

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_CACHE_TAB_MODEL_LIST_OBSERVER_H_
