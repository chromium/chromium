// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"

#import "base/ios/ios_util.h"
#import "base/memory/ptr_util.h"
#import "base/strings/sys_string_conversions.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/previous_session_info/previous_session_info.h"
#import "ios/chrome/browser/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/main/browser.h"
#import "ios/chrome/browser/sessions/session_ios.h"
#import "ios/chrome/browser/sessions/session_ios_factory.h"
#import "ios/chrome/browser/sessions/session_restoration_observer.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/chrome/browser/url/chrome_url_constants.h"
#import "ios/chrome/browser/web/page_placeholder_tab_helper.h"
#import "ios/chrome/browser/web/session_state/web_session_state_tab_helper.h"
#import "ios/chrome/browser/web_state_list/all_web_state_observation_forwarder.h"
#import "ios/chrome/browser/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web_state_list/web_state_list_serialization.h"
#import "ios/chrome/browser/web_state_list/web_usage_enabler/web_usage_enabler_browser_agent.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

BROWSER_USER_DATA_KEY_IMPL(SessionRestorationBrowserAgent)

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
    NSString* session_identifier) {
  DCHECK(session_identifier.length != 0);
  session_identifier_ = session_identifier;
}

NSString* SessionRestorationBrowserAgent::GetSessionID() const {
  DCHECK(session_identifier_.length != 0)
      << "SetSessionID must be called before GetSessionID";
  return session_identifier_;
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

  const int old_count = web_state_list_->count();
  DCHECK_GE(old_count, 0);

  web_state_list_->PerformBatchOperation(
      base::BindOnce(^(WebStateList* web_state_list) {
        web::WebState::CreateParams create_params(browser_state_);
        DeserializeWebStateList(
            web_state_list, window,
            base::BindRepeating(&web::WebState::CreateWithStorageSession,
                                create_params));
      }));

  DCHECK_GT(web_state_list_->count(), old_count);
  int restored_count = web_state_list_->count() - old_count;
  DCHECK_EQ(window.sessions.count, static_cast<NSUInteger>(restored_count));

  std::vector<web::WebState*> restored_web_states;
  restored_web_states.reserve(window.sessions.count);

  std::vector<web::WebState*> web_states_to_remove;
  for (int index = old_count; index < web_state_list_->count(); ++index) {
    web::WebState* web_state = web_state_list_->GetWebStateAt(index);
    if (window.sessions[index - old_count].itemStorages.count == 0) {
      web_states_to_remove.push_back(web_state);
      continue;
    }
    const GURL& visible_url = web_state->GetVisibleURL();

    if (visible_url != kChromeUINewTabURL) {
      PagePlaceholderTabHelper::FromWebState(web_state)
          ->AddPlaceholderForNextNavigation();
    }

    if (visible_url.is_valid()) {
      favicon::WebFaviconDriver::FromWebState(web_state)->FetchFavicon(
          visible_url, /*is_same_document=*/false);
    }

    restored_web_states.push_back(web_state);
  }

  for (web::WebState* web_state_to_remove : web_states_to_remove) {
    const int index = web_state_list_->GetIndexOfWebState(web_state_to_remove);
    DCHECK(index != WebStateList::kInvalidIndex);
    web_state_list_->CloseWebStateAt(index, WebStateList::CLOSE_NO_FLAGS);
  }

  // If there was only one tab and it was the new tab page, clobber it.
  bool closed_ntp_tab = false;
  if (old_count == 1) {
    web::WebState* web_state = web_state_list_->GetWebStateAt(0);

    // An "unrealized" WebState has no pending load. Checking for realization
    // before accessing the NavigationManager prevents accidental realization
    // of the WebState.
    const bool hasPendingLoad =
        web_state->IsRealized() &&
        web_state->GetNavigationManager()->GetPendingItem() != nullptr;

    if (!hasPendingLoad &&
        (web_state->GetLastCommittedURL() == kChromeUINewTabURL)) {
      web_state_list_->CloseWebStateAt(0, WebStateList::CLOSE_USER_ACTION);
      closed_ntp_tab = true;
    }
  }

  for (auto& observer : observers_) {
    observer.SessionRestorationFinished(restored_web_states);
  }
  restoring_session_ = false;
  return closed_ntp_tab;
}

bool SessionRestorationBrowserAgent::RestoreSession() {
  DCHECK(session_identifier_.length != 0);

  PreviousSessionInfo* session_info = [PreviousSessionInfo sharedInstance];
  base::ScopedClosureRunner scoped_restore =
      [session_info startSessionRestoration];

  SessionIOS* session = [session_service_
      loadSessionWithSessionID:session_identifier_
                     directory:browser_state_->GetStatePath()];
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
  DCHECK(session_identifier_.length != 0);

  if (!CanSaveSession())
    return;

  [session_service_ saveSession:session_ios_factory_
                      sessionID:session_identifier_
                      directory:browser_state_->GetStatePath()
                    immediately:immediately];

  for (int i = 0; i < web_state_list_->count(); ++i) {
    web::WebState* web_state = web_state_list_->GetWebStateAt(i);
    WebSessionStateTabHelper::FromWebState(web_state)
        ->SaveSessionStateIfStale();
  }
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

void SessionRestorationBrowserAgent::WebStateDetachedAt(
    WebStateList* web_state_list,
    web::WebState* web_state,
    int index) {
  if (!web_state_list_->empty())
    return;

  // Persist the session state after CloseAllWebStates. SaveSession will discard
  // calls when the web_state_list is not empty and the active WebState is null,
  // which is the order CloseAllWebStates uses.
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

// WebStateObserver methods
void SessionRestorationBrowserAgent::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // Save the session each time a navigation finishes.
  SaveSession(/*immediately=*/false);
}
