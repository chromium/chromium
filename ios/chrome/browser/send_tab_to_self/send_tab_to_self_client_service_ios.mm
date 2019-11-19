// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/send_tab_to_self/send_tab_to_self_client_service_ios.h"

#include <memory>
#include <string>
#include <vector>

#import <Foundation/Foundation.h>

#include "base/logging.h"
#include "components/infobars/core/infobar.h"
#include "components/infobars/core/infobar_manager.h"
#include "components/send_tab_to_self/send_tab_to_self_model.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/infobars/infobar.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/infobars/infobar_utils.h"
#include "ios/chrome/browser/send_tab_to_self/ios_send_tab_to_self_infobar_delegate.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace send_tab_to_self {

SendTabToSelfClientServiceIOS::SendTabToSelfClientServiceIOS(
    ios::ChromeBrowserState* browser_state,
    SendTabToSelfModel* model)
    : model_(model), browser_state_(browser_state) {
  model_->AddObserver(this);
}

SendTabToSelfClientServiceIOS::~SendTabToSelfClientServiceIOS() {
  model_->RemoveObserver(this);
  model_ = nullptr;

  if (web_state_list_) {
    web_state_list_->RemoveObserver(this);
    web_state_list_ = nullptr;
  }

  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

void SendTabToSelfClientServiceIOS::SendTabToSelfModelLoaded() {
  // TODO(crbug.com/949756): Push changes that happened before the model was
  // loaded.
}

void SendTabToSelfClientServiceIOS::EntriesAddedRemotely(
    const std::vector<const SendTabToSelfEntry*>& new_entries) {
  if (new_entries.empty()) {
    return;
  }

  TabModel* tab_model =
      TabModelList::GetLastActiveTabModelForChromeBrowserState(browser_state_);
  if (!tab_model) {
    return;
  }

  WebStateList* web_state_list = tab_model.webStateList;
  if (!web_state_list) {
    return;
  }

  web::WebState* web_state = web_state_list->GetActiveWebState();
  if (!web_state || !web_state->IsVisible()) {
    // If the active WebState is not visible it means the user is in the
    // Tab Grid screen or a Settings page. Register as an observer of the
    // active WebState and WebStateList in order to be notified if the WebState
    // becomes visible again, or if the user changes tab or creates a new tab.
    if (web_state) {
      web_state_ = web_state;
      web_state_->AddObserver(this);
    }

    if (!web_state_list_ || web_state_list_ != web_state_list) {
      if (web_state_list_) {
        web_state_list_->RemoveObserver(this);
      }
      web_state_list_ = web_state_list;
      web_state_list_->AddObserver(this);
    }

    // Pick the most recent entry since only one Infobar can be shown at a time.
    // TODO(crbug.com/944602): Create a function that returns the most recently
    // shared entry.
    entry_ = new_entries.back();

    return;
  }

  // Since we can only show one infobar at the time, pick the most recent entry.
  // TODO(crbug.com/944602): Create a function that returns the most recently
  // shared entry.
  DisplayInfoBar(web_state, new_entries.back());
}

void SendTabToSelfClientServiceIOS::EntriesRemovedRemotely(
    const std::vector<std::string>& guids) {
  NOTIMPLEMENTED();
}

void SendTabToSelfClientServiceIOS::WebStateActivatedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int active_index,
    int reason) {
  DCHECK(entry_);

  // This can happen if the user close the last tab in the tab picker.
  if (!new_web_state) {
    return;
  }

  DisplayInfoBar(new_web_state, entry_);

  CleanUpObserversAndVariables();
}

void SendTabToSelfClientServiceIOS::WasShown(web::WebState* web_state) {
  DCHECK(entry_);
  DCHECK(web_state_);

  DisplayInfoBar(web_state_, entry_);

  CleanUpObserversAndVariables();
}

void SendTabToSelfClientServiceIOS::WebStateDestroyed(
    web::WebState* web_state) {
  DCHECK(web_state_);
  DCHECK(web_state_ == web_state);

  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

void SendTabToSelfClientServiceIOS::DisplayInfoBar(
    web::WebState* web_state,
    const SendTabToSelfEntry* entry) {
  infobars::InfoBarManager* infobar_manager =
      InfoBarManagerImpl::FromWebState(web_state);

  if (!infobar_manager) {
    return;
  }

  infobar_manager->AddInfoBar(CreateConfirmInfoBar(
      IOSSendTabToSelfInfoBarDelegate::Create(entry, model_)));
}

void SendTabToSelfClientServiceIOS::CleanUpObserversAndVariables() {
  DCHECK(web_state_list_);

  entry_ = nullptr;

  web_state_list_->RemoveObserver(this);
  web_state_list_ = nullptr;

  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

}  // namespace send_tab_to_self
