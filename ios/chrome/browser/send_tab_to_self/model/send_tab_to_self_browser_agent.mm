// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_browser_agent.h"

#import <Foundation/Foundation.h>

#import <algorithm>
#import <memory>
#import <string>

#import "base/check.h"
#import "base/containers/span.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "components/infobars/core/infobar.h"
#import "components/infobars/core/infobar_manager.h"
#import "components/send_tab_to_self/metrics_util.h"
#import "components/send_tab_to_self/send_tab_to_self_model.h"
#import "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#import "ios/chrome/browser/infobars/model/infobar_ios.h"
#import "ios/chrome/browser/infobars/model/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/model/infobar_utils.h"
#import "ios/chrome/browser/send_tab_to_self/model/ios_send_tab_to_self_infobar_delegate.h"
#import "ios/chrome/browser/send_tab_to_self/model/send_tab_to_self_load_navigation_user_data.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/sync/model/send_tab_to_self_sync_service_factory.h"
#import "ios/chrome/browser/url_loading/model/url_loading_notifier_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/web/public/web_state.h"

namespace {

// Helper function to remove infobars corresponding to the removed GUIDs from
// the given WebState.
void RemoveInfoBarsForGUIDs(web::WebState* web_state,
                            base::span<const std::string> guids) {
  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);
  if (!infobar_manager) {
    return;
  }

  std::vector<infobars::InfoBar*> infobars_to_remove;
  for (infobars::InfoBar* infobar : infobar_manager->infobars()) {
    if (infobar->GetIdentifier() !=
        infobars::InfoBarDelegate::SEND_TAB_TO_SELF_INFOBAR_DELEGATE) {
      continue;
    }

    auto* delegate =
        static_cast<send_tab_to_self::IOSSendTabToSelfInfoBarDelegate*>(
            infobar->delegate());
    if (std::ranges::contains(guids, delegate->GetGUID())) {
      infobars_to_remove.push_back(infobar);
    }
  }

  for (infobars::InfoBar* infobar : infobars_to_remove) {
    infobar_manager->RemoveInfoBar(infobar);
  }
}

}  // namespace

SendTabToSelfBrowserAgent::SendTabToSelfBrowserAgent(Browser* browser)
    : BrowserUserData(browser),
      model_(
          SendTabToSelfSyncServiceFactory::GetForProfile(browser_->GetProfile())
              ->GetSendTabToSelfModel()) {
  browser_observation_.Observe(browser_);
  model_observation_.Observe(model_.get());
  UrlLoadingNotifierBrowserAgent* loading_notifier =
      UrlLoadingNotifierBrowserAgent::FromBrowser(browser_);
  if (loading_notifier) {
    url_loading_observation_.Observe(loading_notifier);
  }
}

SendTabToSelfBrowserAgent::~SendTabToSelfBrowserAgent() = default;

void SendTabToSelfBrowserAgent::BrowserDestroyed(Browser* browser) {
  url_loading_observation_.Reset();
  model_observation_.Reset();
  browser_observation_.Reset();
  CleanUpObserversAndVariables();
}

void SendTabToSelfBrowserAgent::OnEntriesAddedRemotely(
    base::span<const send_tab_to_self::SendTabToSelfEntry* const> new_entries) {
  DisplayNewEntries(new_entries);
}

void SendTabToSelfBrowserAgent::OnEntriesRemovedRemotely(
    base::span<const std::string> guids) {
  DismissEntries(guids);
}

#pragma mark - ReceivingUiHandler

void SendTabToSelfBrowserAgent::DisplayNewEntries(
    base::span<const send_tab_to_self::SendTabToSelfEntry* const> new_entries) {
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
      web_state_observation_.Observe(pending_web_state_.get());
    }

    if (!web_state_list_observation_.IsObserving()) {
      web_state_list_observation_.Observe(browser_->GetWebStateList());
    }

    // Pick the most recent entry since only one Infobar can be shown at a time.
    // TODO(crbug.com/40619532): Create a function that returns the most
    // recently shared entry.
    pending_entry_ = new_entries.back();

    return;
  }

  // Since we can only show one infobar at the time, pick the most recent entry.
  // TODO(crbug.com/40619532): Create a function that returns the most recently
  // shared entry.
  DisplayInfoBar(web_state, new_entries.back());
}

void SendTabToSelfBrowserAgent::DismissEntries(
    base::span<const std::string> guids) {
  if (guids.empty()) {
    return;
  }

  if (pending_entry_ &&
      std::ranges::contains(guids, pending_entry_->GetGUID())) {
    CleanUpObserversAndVariables();
  }

  WebStateList* web_state_list = browser_->GetWebStateList();
  for (int i = 0; i < web_state_list->count(); ++i) {
    RemoveInfoBarsForGUIDs(web_state_list->GetWebStateAt(i), guids);
  }
}

#pragma mark - WebStateListObserver

void SendTabToSelfBrowserAgent::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  // The active WebState can be null if the user close the last tab in the tab
  // picker.
  if (!status.active_web_state_change() || !status.new_active_web_state) {
    return;
  }

  DCHECK(pending_entry_);
  DisplayInfoBar(status.new_active_web_state, pending_entry_);
  CleanUpObserversAndVariables();
}

#pragma mark - WebStateObserver

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
      send_tab_to_self::IOSSendTabToSelfInfoBarDelegate::Create(
          entry, model_,
          HandlerForProtocol(browser_->GetCommandDispatcher(),
                             SceneCommands))));
}

void SendTabToSelfBrowserAgent::CleanUpObserversAndVariables() {
  pending_entry_ = nullptr;

  web_state_list_observation_.Reset();

  web_state_observation_.Reset();
  pending_web_state_ = nullptr;
}

void SendTabToSelfBrowserAgent::TabWillLoadUrl(
    const UrlLoadParams& params,
    base::WeakPtr<web::WebState> web_state) {
  if (!web_state) {
    return;
  }
  // Always remove old data to ensure we don't use stale GUIDs.
  SendTabToSelfLoadNavigationUserData::RemoveFromWebState(web_state.get());

  if (params.is_from_send_tab_to_self()) {
    SendTabToSelfLoadNavigationUserData::CreateForWebState(
        web_state.get(), params.send_tab_to_self_entry_guid);
  }
}
