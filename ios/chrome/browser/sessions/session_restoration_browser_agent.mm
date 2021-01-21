// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/sessions/session_restoration_browser_agent.h"

#include "base/memory/ptr_util.h"
#include "base/strings/sys_string_conversions.h"
#include "components/favicon/ios/web_favicon_driver.h"
#import "components/previous_session_info/previous_session_info.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/sessions/session_ios.h"
#import "ios/chrome/browser/sessions/session_ios_factory.h"
#import "ios/chrome/browser/sessions/session_restoration_observer.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/chrome/browser/ui/util/multi_window_support.h"
#import "ios/chrome/browser/web/page_placeholder_tab_helper.h"
#import "ios/chrome/browser/web_state_list/all_web_state_observation_forwarder.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_serialization.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_usage_enabler_browser_agent.h"
#include "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#include "ios/web/public/security/certificate_policy_cache.h"
#import "ios/web/public/session/serializable_user_data_manager.h"
#include "ios/web/public/session/session_certificate_policy_cache.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BROWSER_USER_DATA_KEY_IMPL(SessionRestorationBrowserAgent)

namespace {
const std::string kSessionDirectory = "Sessions";
}

// static
void SessionRestorationBrowserAgent::CreateForBrowser(
    Browser* browser,
    SessionServiceIOS* session_service) {
  DCHECK(browser);
  if (!FromBrowser(browser)) {
    browser->SetUserData(UserDataKey(),
                         base::WrapUnique(new SessionRestorationBrowserAgent(
                             browser, session_service)));
  }
}

SessionRestorationBrowserAgent::SessionRestorationBrowserAgent(
    Browser* browser,
    SessionServiceIOS* session_service)
    : session_service_(session_service),
      web_state_list_(browser->GetWebStateList()),
      web_enabler_(WebUsageEnablerBrowserAgent::FromBrowser(browser)),
      browser_state_(browser->GetBrowserState()),
      session_ios_factory_(
          [[SessionIOSFactory alloc] initWithWebStateList:web_state_list_]),
      all_web_state_observer_(
          std::make_unique<AllWebStateObservationForwarder>(web_state_list_,
                                                            this)) {
  browser->AddObserver(this);
  web_state_list_->AddObserver(this);
}

SessionRestorationBrowserAgent::~SessionRestorationBrowserAgent() {
  // Disconnect the session factory object as it's not granteed that it will be
  // released before it's referenced by the session service.
  [session_ios_factory_ disconnect];
}

void SessionRestorationBrowserAgent::SetSessionID(
    const std::string& session_identifier) {
  session_identifier_ = session_identifier;
}

void SessionRestorationBrowserAgent::AddObserver(
    SessionRestorationObserver* observer) {
  observers_.AddObserver(observer);
}

void SessionRestorationBrowserAgent::RemoveObserver(
    SessionRestorationObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool SessionRestorationBrowserAgent::RestoreSessionWindow(
    SessionWindowIOS* window) {
  if (!window.sessions.count)
    return false;
  restoring_session_ = true;

  for (auto& observer : observers_) {
    observer.WillStartSessionRestoration();
  }

  int old_count = web_state_list_->count();
  DCHECK_GE(old_count, 0);

  web_state_list_->PerformBatchOperation(base::BindOnce(^(
      WebStateList* web_state_list) {
    // Don't trigger the initial load for these restored WebStates since the
    // number of WKWebViews is unbounded and may lead to an OOM crash.
    const bool saved_triggers_initial_load =
        web_enabler_->TriggersInitialLoad();
    web_enabler_->SetTriggersInitialLoad(false);
    web::WebState::CreateParams createParams(browser_state_);
    DeserializeWebStateList(
        web_state_list, window,
        base::BindRepeating(&web::WebState::CreateWithStorageSession,
                            createParams));
    web_enabler_->SetTriggersInitialLoad(saved_triggers_initial_load);
  }));

  DCHECK_GT(web_state_list_->count(), old_count);
  int restored_count = web_state_list_->count() - old_count;
  DCHECK_EQ(window.sessions.count, static_cast<NSUInteger>(restored_count));

  scoped_refptr<web::CertificatePolicyCache> policy_cache =
      web::BrowserState::GetCertificatePolicyCache(browser_state_);

  std::vector<web::WebState*> restored_web_states;
  restored_web_states.reserve(window.sessions.count);

  for (int index = old_count; index < web_state_list_->count(); ++index) {
    web::WebState* web_state = web_state_list_->GetWebStateAt(index);
    web::NavigationItem* visible_item =
        web_state->GetNavigationManager()->GetVisibleItem();

    if (!(visible_item &&
          visible_item->GetVirtualURL() == kChromeUINewTabURL)) {
      PagePlaceholderTabHelper::FromWebState(web_state)
          ->AddPlaceholderForNextNavigation();
    }

    if (visible_item && visible_item->GetVirtualURL().is_valid()) {
      favicon::WebFaviconDriver::FromWebState(web_state)->FetchFavicon(
          visible_item->GetVirtualURL(), /*is_same_document=*/false);
    }

    // Restore the CertificatePolicyCache (note that webState is invalid after
    // passing it via move semantic to -initWithWebState:model:).
    web_state->GetSessionCertificatePolicyCache()->UpdateCertificatePolicyCache(
        policy_cache);

    restored_web_states.push_back(web_state);
  }

  // If there was only one tab and it was the new tab page, clobber it.
  bool closed_ntp_tab = false;
  if (old_count == 1) {
    web::WebState* webState = web_state_list_->GetWebStateAt(0);
    bool hasPendingLoad =
        webState->GetNavigationManager()->GetPendingItem() != nullptr;
    if (!hasPendingLoad &&
        webState->GetLastCommittedURL() == kChromeUINewTabURL) {
      web_state_list_->CloseWebStateAt(0, WebStateList::CLOSE_USER_ACTION);

      closed_ntp_tab = true;
      old_count = 0;
    }
  }
  for (auto& observer : observers_) {
    observer.SessionRestorationFinished(restored_web_states);
  }
  restoring_session_ = false;
  return closed_ntp_tab;
}

bool SessionRestorationBrowserAgent::RestoreSession() {
  PreviousSessionInfo* session_info = [PreviousSessionInfo sharedInstance];
  auto scoped_restore = [session_info startSessionRestoration];

  const base::FilePath& path = browser_state_->GetStatePath();
  NSString* session_id =
      (IsMultiwindowSupported() && session_info.isMultiWindowEnabledSession)
          ? base::SysUTF8ToNSString(session_identifier_)
          : nil;

  SessionIOS* session = [session_service_
      loadSessionWithSessionID:session_id
                     directory:base::SysUTF8ToNSString(path.AsUTF8Unsafe())];
  SessionWindowIOS* session_window = nil;

  if (session) {
    DCHECK_EQ(session.sessionWindows.count, 1u);
    session_window = session.sessionWindows[0];
  }

  return RestoreSessionWindow(session_window);
}

bool SessionRestorationBrowserAgent::IsRestoringSession() {
  return restoring_session_;
}

void SessionRestorationBrowserAgent::SaveSession(bool immediately) {
  if (!CanSaveSession())
    return;

  const base::FilePath& path = browser_state_->GetStatePath();
  [session_service_ saveSession:session_ios_factory_
                      sessionID:base::SysUTF8ToNSString(session_identifier_)
                      directory:base::SysUTF8ToNSString(path.AsUTF8Unsafe())
                    immediately:immediately];
}

bool SessionRestorationBrowserAgent::CanSaveSession() {
  // A session requires an active browser state and web state list.
  if (!browser_state_ || !web_state_list_)
    return NO;
  // Sessions where there's no active tab shouldn't be saved, unless the web
  // state list is empty. This is a transitional state.
  if (!web_state_list_->empty() && !web_state_list_->GetActiveWebState())
    return NO;

  return YES;
}

// Browser Observer methods:
void SessionRestorationBrowserAgent::BrowserDestroyed(Browser* browser) {
  DCHECK_EQ(browser->GetWebStateList(), web_state_list_);
  // Stop observing web states.
  all_web_state_observer_.reset();
  // Stop observing web state list.
  browser->GetWebStateList()->RemoveObserver(this);
  browser->RemoveObserver(this);
}

// WebStateList Observer methods:
void SessionRestorationBrowserAgent::WebStateActivatedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int active_index,
    ActiveWebStateChangeReason reason) {
  if (new_web_state && new_web_state->IsLoading())
    return;

  // Persist the session state if the new web state is not loading (or if
  // the last tab was closed).
  SaveSession(/*immediately=*/false);
}

void SessionRestorationBrowserAgent::WillDetachWebStateAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index) {
  if (web_state_list->active_index() == index)
    return;

  // Persist the session state if a background tab is detached.
  SaveSession(/*immediately=*/false);
}

void SessionRestorationBrowserAgent::WebStateInsertedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index,
    bool activating) {
  if (activating || web_state->IsLoading())
    return;

  // Persist the session state if the new web state is not loading.
  SaveSession(/*immediately=*/false);
}

void SessionRestorationBrowserAgent::WebStateReplacedAt(
    WebStateList* web_state_list,
    web::WebState* old_web_state,
    web::WebState* new_web_state,
    int index) {
  if (new_web_state->IsLoading())
    return;

  // Persist the session state if the new web state is not loading.
  SaveSession(/*immediately=*/false);
}

void SessionRestorationBrowserAgent::WebStateMoved(WebStateList* web_state_list,
                                                   web::WebState* web_state,
                                                   int from_index,
                                                   int to_index) {
  if (web_state->IsLoading())
    return;

  // Persist the session state if the new web state is not loading.
  SaveSession(/*immediately=*/false);
}

base::FilePath SessionRestorationBrowserAgent::GetSessionStoragePath(
    bool force_single_window) {
  base::FilePath path = browser_state_->GetStatePath();
  if (!force_single_window && IsMultiwindowSupported() &&
      !session_identifier_.empty()) {
    path = path.Append(kSessionDirectory)
               .Append(session_identifier_)
               .AsEndingWithSeparator();
  }

  return path;
}

// WebStateObserver methods
void SessionRestorationBrowserAgent::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // Save the session each time a navigation finishes.
  SaveSession(/*immediately=*/false);
}
