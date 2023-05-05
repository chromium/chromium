// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/upgrade/upgrade_center_browser_agent.h"

#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/upgrade/upgrade_center.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BROWSER_USER_DATA_KEY_IMPL(UpgradeCenterBrowserAgent)

UpgradeCenterBrowserAgent::UpgradeCenterBrowserAgent(
    Browser* browser,
    UpgradeCenter* upgrade_center)
    : upgrade_center_(upgrade_center) {
  DCHECK(browser);
  DCHECK(upgrade_center);
  browser->AddObserver(this);
  browser->GetWebStateList()->AddObserver(this);
}

UpgradeCenterBrowserAgent::~UpgradeCenterBrowserAgent() {}

void UpgradeCenterBrowserAgent::BrowserDestroyed(Browser* browser) {
  DCHECK(browser);
  browser->GetWebStateList()->RemoveObserver(this);
  browser->RemoveObserver(this);
}

void UpgradeCenterBrowserAgent::WebStateInsertedAt(WebStateList* web_state_list,
                                                   web::WebState* web_state,
                                                   int index,
                                                   bool activating) {
  DCHECK(web_state);

  // When adding new tabs, check what kind of reminder infobar should
  // be added to the new tab. Try to add only one of them.
  // This check is done when a new tab is added either through the Tools Menu
  // "New Tab", through a long press on the Tab Switcher button "New Tab", and
  // through creating a New Tab from the Tab Switcher. This logic needs to
  // happen after a new WebState has added and finished initial navigation. If
  // this happens earlier, the initial navigation may end up clearing the
  // infobar(s) that are just added.
  infobars::InfoBarManager* info_bar_manager =
      InfoBarManagerImpl::FromWebState(web_state);
  NSString* tab_id = web_state->GetStableIdentifier();

  [upgrade_center_ addInfoBarToManager:info_bar_manager forTabId:tab_id];
}

void UpgradeCenterBrowserAgent::WillDetachWebStateAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index) {
  DCHECK(web_state);

  [upgrade_center_ tabWillClose:web_state->GetStableIdentifier()];
}
