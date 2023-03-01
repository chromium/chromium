// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/send_tab_to_self_browser_agent.h"

#import <memory>
#import <string>
#import <vector>

#import <Foundation/Foundation.h>

#import "base/check.h"
#import "base/notreached.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/infobars/core/infobar.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/send_tab_to_self/metrics_util.h"
#import "components/send_tab_to_self/send_tab_to_self_model.h"
#import "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/infobars/infobar_ios.h"
#import "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/infobar_utils.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/send_tab_to_self/ios_send_tab_to_self_infobar_delegate.h"
#import "ios/chrome/browser/sync/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BROWSER_USER_DATA_KEY_IMPL(SendTabToSelfBrowserAgent)

SendTabToSelfBrowserAgent::SendTabToSelfBrowserAgent(Browser* browser)
    : browser_(browser),
      model_(SendTabToSelfSyncServiceFactory::GetForBrowserState(
                 browser_->GetBrowserState())
                 ->GetSendTabToSelfModel()) {
  model_observation_.Observe(model_);
  browser_observation_.Observe(browser_);
}

SendTabToSelfBrowserAgent::~SendTabToSelfBrowserAgent() = default;

void SendTabToSelfBrowserAgent::SendTabToSelfModelLoaded() {
  // TODO(crbug.com/949756): Push changes that happened before the model was
  // loaded.
}

void SendTabToSelfBrowserAgent::EntriesAddedRemotely(
    const std::vector<const send_tab_to_self::SendTabToSelfEntry*>&
        new_entries) {
  if (new_entries.empty()) {
    return;
  }

  web::WebState* web_state = browser_->GetWebStateList()->GetActiveWebState();
  if (!web_state || !web_state->IsVisible()) {
    // If the active WebState is not visible it means the user is in the
    // Tab Grid screen or a Settings page. Register as an observer of the
    // active WebState and WebStateList in order to be notified if the WebState
    // becomes visible again, or if the user changes tab or creates a new tab.
    if (web_state) {
      pending_web_state_ = web_state;
      web_state_observation_.Reset();
      web_state_observation_.Observe(pending_web_state_);
    }

    if (!web_state_list_observation_.IsObserving()) {
      web_state_list_observation_.Observe(browser_->GetWebStateList());
    }

    // Pick the most recent entry since only one Infobar can be shown at a time.
    // TODO(crbug.com/944602): Create a function that returns the most recently
    // shared entry.
    pending_entry_ = new_entries.back();

    return;
  }

  // Since we can only show one infobar at the time, pick the most recent entry.
  // TODO(crbug.com/944602): Create a function that returns the most recently
  // shared entry.
  DisplayInfoBar(web_state, new_entries.back());
}

void SendTabToSelfBrowserAgent::EntriesRemovedRemotely(
    const std::vector<std::string>& guids) {
  NOTIMPLEMENTED();
}

void SendTabToSelfBrowserAgent::WebStateActivatedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int active_index,
    ActiveWebStateChangeReason reason) {
  DCHECK(pending_entry_);

  // This can happen if the user close the last tab in the tab picker.
  if (!new_web_state) {
    return;
  }

  DisplayInfoBar(new_web_state, pending_entry_);

  CleanUpObserversAndVariables();
}

void SendTabToSelfBrowserAgent::WasShown(web::WebState* web_state) {
  DCHECK(pending_entry_);
  DCHECK(pending_web_state_);

  DisplayInfoBar(pending_web_state_, pending_entry_);

  CleanUpObserversAndVariables();
}

void SendTabToSelfBrowserAgent::WebStateDestroyed(web::WebState* web_state) {
  DCHECK(pending_web_state_);
  DCHECK(pending_web_state_ == web_state);

  web_state_observation_.Reset();
  pending_web_state_ = nullptr;
}

void SendTabToSelfBrowserAgent::BrowserDestroyed(Browser* browser) {
  model_observation_.Reset();

  web_state_list_observation_.Reset();

  web_state_observation_.Reset();

  browser_observation_.Reset();
}

void SendTabToSelfBrowserAgent::DisplayInfoBar(
    web::WebState* web_state,
    const send_tab_to_self::SendTabToSelfEntry* entry) {
  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);

  if (!infobar_manager) {
    return;
  }

  send_tab_to_self::RecordNotificationShown();

  infobar_manager->AddInfoBar(CreateConfirmInfoBar(
      send_tab_to_self::IOSSendTabToSelfInfoBarDelegate::Create(entry,
                                                                model_)));
}

void SendTabToSelfBrowserAgent::CleanUpObserversAndVariables() {
  pending_entry_ = nullptr;

  web_state_list_observation_.Reset();

  web_state_observation_.Reset();
  pending_web_state_ = nullptr;
}
