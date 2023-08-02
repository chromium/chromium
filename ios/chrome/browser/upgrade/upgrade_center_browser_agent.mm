// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/upgrade/upgrade_center_browser_agent.h"

#import "base/check.h"
#import "base/notreached.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/upgrade/upgrade_center.h"

BROWSER_USER_DATA_KEY_IMPL(UpgradeCenterBrowserAgent)

UpgradeCenterBrowserAgent::UpgradeCenterBrowserAgent(
    Browser* browser,
    UpgradeCenter* upgrade_center)
    : upgrade_center_(upgrade_center) {
  CHECK(browser);
  CHECK(upgrade_center);
  CHECK(browser->GetWebStateList()->empty());
  web_state_list_observation_.Observe(browser->GetWebStateList());
}

UpgradeCenterBrowserAgent::~UpgradeCenterBrowserAgent() {}

#pragma mark - WebStateListObserver

void UpgradeCenterBrowserAgent::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // Do nothing when a WebState is selected and its status is updated.
      break;
    case WebStateListChange::Type::kDetach: {
      const WebStateListChangeDetach& detach_change =
          change.As<WebStateListChangeDetach>();
      WebStateDetached(detach_change.detached_web_state());
      break;
    }
    case WebStateListChange::Type::kMove:
      // Do nothing when a WebState is moved.
      break;
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace_change =
          change.As<WebStateListChangeReplace>();
      WebStateDetached(replace_change.replaced_web_state());
      WebStateAttached(replace_change.inserted_web_state());
      break;
    }
    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insert_change =
          change.As<WebStateListChangeInsert>();
      WebStateAttached(insert_change.inserted_web_state());
      break;
    }
  }
}

void UpgradeCenterBrowserAgent::WebStateListDestroyed(
    WebStateList* web_state_list) {
  web_state_observations_.RemoveAllObservations();
  web_state_list_observation_.Reset();
}

void UpgradeCenterBrowserAgent::WebStateRealized(web::WebState* web_state) {
  CHECK(web_state);
  web_state_observations_.RemoveObservation(web_state);
  WebStateAttached(web_state);
}

void UpgradeCenterBrowserAgent::WebStateDestroyed(web::WebState* web_state) {
  // UpgradeCenterBrowserAgent should stop observing all WebState before
  // they are destroyed, by observing when they are removed from the
  // WebStateList. It is an error in UpgradeCenterBrowserAgent if this
  // method is called.
  NOTREACHED_NORETURN();
}

void UpgradeCenterBrowserAgent::WebStateAttached(web::WebState* web_state) {
  CHECK(web_state);
  if (!web_state->IsRealized()) {
    web_state_observations_.AddObservation(web_state);
    return;
  }

  // When adding new tabs, check what kind of reminder infobar should
  // be added to the new tab. Try to add only one of them.
  // This check is done when a new tab is added either through the Tools Menu
  // "New Tab", through a long press on the Tab Switcher button "New Tab", and
  // through creating a New Tab from the Tab Switcher. This logic needs to
  // happen after a new WebState has added and finished initial navigation. If
  // this happens earlier, the initial navigation may end up clearing the
  // infobar(s) that are just added.
  [upgrade_center_
      addInfoBarToManager:InfoBarManagerImpl::FromWebState(web_state)
                 forTabId:web_state->GetStableIdentifier()];
}

void UpgradeCenterBrowserAgent::WebStateDetached(web::WebState* web_state) {
  CHECK(web_state);
  if (!web_state->IsRealized()) {
    web_state_observations_.RemoveObservation(web_state);
    return;
  }

  [upgrade_center_ tabWillClose:web_state->GetStableIdentifier()];
}
