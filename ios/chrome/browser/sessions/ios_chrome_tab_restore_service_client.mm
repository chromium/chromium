// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sessions/ios_chrome_tab_restore_service_client.h"

#include "base/bind.h"
#include "base/callback.h"
#include "components/sessions/ios/ios_live_tab.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#include "ios/chrome/browser/sessions/tab_restore_service_delegate_impl_ios.h"
#include "ios/chrome/browser/sessions/tab_restore_service_delegate_impl_ios_factory.h"
#import "ios/chrome/browser/tabs/tab_model.h"
#import "ios/chrome/browser/tabs/tab_model_list.h"
#include "ios/chrome/browser/tabs/tab_model_synced_window_delegate.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#include "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
sessions::LiveTabContext* FindLiveTabContextWithCondition(
    base::RepeatingCallback<bool(TabModel*)> condition) {
  std::vector<ios::ChromeBrowserState*> browser_states =
      GetApplicationContext()
          ->GetChromeBrowserStateManager()
          ->GetLoadedBrowserStates();

  for (ios::ChromeBrowserState* browser_state : browser_states) {
    DCHECK(!browser_state->IsOffTheRecord());
    NSArray<TabModel*>* tab_models;

    tab_models = TabModelList::GetTabModelsForChromeBrowserState(browser_state);
    for (TabModel* tab_model : tab_models) {
      if (condition.Run(tab_model)) {
        return TabRestoreServiceDelegateImplIOSFactory::GetForBrowserState(
            browser_state);
      }
    }

    if (!browser_state->HasOffTheRecordChromeBrowserState())
      continue;

    ios::ChromeBrowserState* otr_browser_state =
        browser_state->GetOffTheRecordChromeBrowserState();

    tab_models =
        TabModelList::GetTabModelsForChromeBrowserState(otr_browser_state);
    for (TabModel* tab_model : tab_models) {
      if (condition.Run(tab_model)) {
        return TabRestoreServiceDelegateImplIOSFactory::GetForBrowserState(
            browser_state);
      }
    }
  }

  return nullptr;
}
}  // namespace

IOSChromeTabRestoreServiceClient::IOSChromeTabRestoreServiceClient(
    ios::ChromeBrowserState* browser_state)
    : browser_state_(browser_state) {}

IOSChromeTabRestoreServiceClient::~IOSChromeTabRestoreServiceClient() {}

sessions::LiveTabContext*
IOSChromeTabRestoreServiceClient::CreateLiveTabContext(
    const std::string& /* app_name */,
    const gfx::Rect& /* bounds */,
    ui::WindowShowState /* show_state */,
    const std::string& /* workspace */) {
  return TabRestoreServiceDelegateImplIOSFactory::GetForBrowserState(
      browser_state_);
}

sessions::LiveTabContext*
IOSChromeTabRestoreServiceClient::FindLiveTabContextForTab(
    const sessions::LiveTab* tab) {
  const sessions::IOSLiveTab* requested_tab =
      static_cast<const sessions::IOSLiveTab*>(tab);

  return FindLiveTabContextWithCondition(base::Bind(
      [](const web::WebState* web_state, TabModel* tab_model) {
        WebStateList* web_state_list = tab_model.webStateList;
        const int index = web_state_list->GetIndexOfWebState(web_state);
        return index != WebStateList::kInvalidIndex;
      },
      requested_tab->web_state()));
}

sessions::LiveTabContext*
IOSChromeTabRestoreServiceClient::FindLiveTabContextWithID(
    SessionID desired_id) {
  return FindLiveTabContextWithCondition(base::Bind(
      [](SessionID desired_id, TabModel* tab_model) {
        DCHECK(tab_model.syncedWindowDelegate);
        return tab_model.syncedWindowDelegate->GetSessionId() == desired_id;
      },
      desired_id));
}

bool IOSChromeTabRestoreServiceClient::ShouldTrackURLForRestore(
    const GURL& url) {
  // NOTE: In the //chrome client, chrome://quit and chrome://restart are
  // blacklisted from being tracked to avoid entering infinite loops. However,
  // iOS intentionally does not support those URLs, so there is no need to
  // blacklist them here.
  return url.is_valid();
}

std::string IOSChromeTabRestoreServiceClient::GetExtensionAppIDForTab(
    sessions::LiveTab* tab) {
  return std::string();
}

base::FilePath IOSChromeTabRestoreServiceClient::GetPathToSaveTo() {
  // Note that this will return a different path in incognito from normal mode.
  // In this case, that shouldn't be an issue because the tab restore service
  // is not used in incognito mode.
  return browser_state_->GetStatePath();
}

GURL IOSChromeTabRestoreServiceClient::GetNewTabURL() {
  return GURL(kChromeUINewTabURL);
}

bool IOSChromeTabRestoreServiceClient::HasLastSession() {
  return false;
}

void IOSChromeTabRestoreServiceClient::GetLastSession(
    const sessions::GetLastSessionCallback& callback,
    base::CancelableTaskTracker* tracker) {
  NOTREACHED();
}
