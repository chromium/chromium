// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/session_restoration_browser_agent.h"

#import "base/ios/ios_util.h"
#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/favicon/ios/web_favicon_driver.h"
#import "components/previous_session_info/previous_session_info.h"
#import "ios/chrome/browser/sessions/session_ios.h"
#import "ios/chrome/browser/sessions/session_ios_factory.h"
#import "ios/chrome/browser/sessions/session_restoration_observer.h"
#import "ios/chrome/browser/sessions/session_service_ios.h"
#import "ios/chrome/browser/sessions/session_window_ios.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser_state/chrome_browser_state.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/all_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/web/features.h"
#import "ios/chrome/browser/web/page_placeholder_tab_helper.h"
#import "ios/chrome/browser/web/session_state/web_session_state_tab_helper.h"
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
    SessionServiceIOS* session_service,
    bool enable_pinned_web_states)
    : session_service_(session_service),
      web_state_list_(browser->GetWebStateList()),
      web_enabler_(WebUsageEnablerBrowserAgent::FromBrowser(browser)),
      browser_state_(browser->GetBrowserState()),
      session_ios_factory_(
          [[SessionIOSFactory alloc] initWithWebStateList:web_state_list_]),
      enable_pinned_web_states_(enable_pinned_web_states),
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
    SessionWindowIOS* window,
    SessionRestorationScope scope) {
  // Start the session restoration.
  restoring_session_ = true;

  for (auto& observer : observers_) {
    observer.WillStartSessionRestoration();
  }

  const int old_count = web_state_list_->count();
  const int old_first_non_pinned =
      web_state_list_->GetIndexOfFirstNonPinnedWebState();
  DCHECK_GE(old_count, 0);

  web_state_list_->PerformBatchOperation(
      base::BindOnce(^(WebStateList* web_state_list) {
        web::WebState::CreateParams create_params(browser_state_);
        DeserializeWebStateList(
            web_state_list, window, scope, enable_pinned_web_states_,
            base::BindRepeating(&web::WebState::CreateWithStorageSession,
                                create_params));
      }));

  DCHECK_GE(web_state_list_->count(), old_count);
  int restored_count = web_state_list_->count() - old_count;
  int restored_pinned_count =
      web_state_list_->GetIndexOfFirstNonPinnedWebState() -
      old_first_non_pinned;

  NSArray<CRWSessionStorage*>* restored_session_storages =
      GetRestoredSessionStoragesForScope(scope, window.sessions,
                                         restored_count);
  DCHECK_EQ(restored_session_storages.count,
            static_cast<NSUInteger>(restored_count));

  std::vector<web::WebState*> restored_web_states;
  restored_web_states.reserve(restored_count);

  std::vector<web::WebState*> web_states_to_remove;
  web_states_to_remove.reserve(restored_count);

  // Find restored pinned WebStates.
  for (int index = old_first_non_pinned;
       index < web_state_list_->GetIndexOfFirstNonPinnedWebState(); ++index) {
    web::WebState* web_state = web_state_list_->GetWebStateAt(index);

    const int session_index = index - old_first_non_pinned;
    DCHECK_EQ(restored_session_storages[session_index].uniqueIdentifier,
              web_state->GetUniqueIdentifier());

    if (restored_session_storages[session_index].itemStorages.count > 0) {
      restored_web_states.push_back(web_state);
    } else {
      web_states_to_remove.push_back(web_state);
    }
  }

  // Find restored non-pinned WebStates.
  for (int index = old_count + restored_pinned_count;
       index < web_state_list_->count(); ++index) {
    web::WebState* web_state = web_state_list_->GetWebStateAt(index);

    const int session_index = index - old_count;
    DCHECK_EQ(restored_session_storages[session_index].uniqueIdentifier,
              web_state->GetUniqueIdentifier());

    if (restored_session_storages[session_index].itemStorages.count > 0) {
      restored_web_states.push_back(web_state);
    } else {
      web_states_to_remove.push_back(web_state);
    }
  }

  // Do not count WebState that are going to be removed.
  restored_count -= web_states_to_remove.size();

  DCHECK_EQ(restored_web_states.size(),
            static_cast<unsigned long>(restored_count));

  // Iterating backwards to avoid messing up the indexes.
  for (int index = restored_count - 1; index >= 0; --index) {
    web::WebState* web_state = restored_web_states[index];

    const GURL& visible_url = web_state->GetVisibleURL();

    if (visible_url != kChromeUINewTabURL) {
      PagePlaceholderTabHelper::FromWebState(web_state)
          ->AddPlaceholderForNextNavigation();
    }

    if (visible_url.is_valid()) {
      favicon::WebFaviconDriver::FromWebState(web_state)->FetchFavicon(
          visible_url, /*is_same_document=*/false);
    }
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
    const bool has_pending_load =
        web_state->IsRealized() &&
        web_state->GetNavigationManager()->GetPendingItem() != nullptr;

    if (!has_pending_load &&
        (web_state->GetLastCommittedURL() == kChromeUINewTabURL)) {
      web_state_list_->CloseWebStateAt(0, WebStateList::CLOSE_USER_ACTION);
      closed_ntp_tab = true;
    }
  }

  for (auto& observer : observers_) {
    observer.SessionRestorationFinished(restored_web_states);
  }

  // Session restoration is complete.
  restoring_session_ = false;

  // Schedule a session save.
  SaveSession(/*immediately*/ false);

  return closed_ntp_tab;
}

bool SessionRestorationBrowserAgent::RestoreSession() {
  DCHECK(session_identifier_.length != 0);

  const base::TimeTicks start_time = base::TimeTicks::Now();

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

  const bool closed_ntp_tab =
      RestoreSessionWindow(session_window, SessionRestorationScope::kAll);

  base::UmaHistogramTimes("Session.WebStates.LoadingTimeOnMainThread",
                          base::TimeTicks::Now() - start_time);

  return closed_ntp_tab;
}

bool SessionRestorationBrowserAgent::IsRestoringSession() {
  return restoring_session_;
}

void SessionRestorationBrowserAgent::SaveSession(bool immediately) {
  DCHECK(session_identifier_.length != 0);

  if (!CanSaveSession())
    return;

  if (batch_in_progress_) {
    save_after_batch_ = true;
    save_immediately_ = save_immediately_ || immediately;
    return;
  }

  [session_service_ saveSession:session_ios_factory_
                      sessionID:session_identifier_
                      directory:browser_state_->GetStatePath()
                    immediately:immediately];

  if (web::UseNativeSessionRestorationCache()) {
    for (int i = 0; i < web_state_list_->count(); ++i) {
      web::WebState* web_state = web_state_list_->GetWebStateAt(i);
      WebSessionStateTabHelper::FromWebState(web_state)
          ->SaveSessionStateIfStale();
    }
  }
}

NSArray<CRWSessionStorage*>*
SessionRestorationBrowserAgent::GetRestoredSessionStoragesForScope(
    SessionRestorationScope scope,
    NSArray<CRWSessionStorage*>* session_storages,
    int restored_count) {
  NSRange restored_sessions_range;

  switch (scope) {
    case SessionRestorationScope::kPinnedOnly:
    case SessionRestorationScope::kAll:
      restored_sessions_range = NSMakeRange(0, restored_count);
      break;
    case SessionRestorationScope::kRegularOnly:
      restored_sessions_range =
          NSMakeRange(session_storages.count - restored_count, restored_count);
      break;
  }

  return [session_storages subarrayWithRange:restored_sessions_range];
}

bool SessionRestorationBrowserAgent::CanSaveSession() {
  // Do not schedule a save while a session restoration is in progress.
  if (restoring_session_) {
    return false;
  }

  // A session requires an active browser state and web state list.
  if (!browser_state_ || !web_state_list_) {
    return false;
  }

  // Sessions where there's no active tab shouldn't be saved, unless the web
  // state list is empty. This is a transitional state.
  if (!web_state_list_->empty() && !web_state_list_->GetActiveWebState()) {
    return false;
  }

  return true;
}

#pragma mark - BrowserObserver

void SessionRestorationBrowserAgent::BrowserDestroyed(Browser* browser) {
  DCHECK_EQ(browser->GetWebStateList(), web_state_list_);
  // Stop observing web states.
  all_web_state_observer_.reset();
  // Stop observing web state list.
  browser->GetWebStateList()->RemoveObserver(this);
  browser->RemoveObserver(this);
}

#pragma mark - WebStateListObserver

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

void SessionRestorationBrowserAgent::WebStateListChanged(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateSelection& selection) {
  switch (change.type()) {
    case WebStateListChange::Type::kSelectionOnly:
      // TODO(crbug.com/1442546): Move the implementation from
      // WebStateActivatedAt() to here. Note that here is reachable only when
      // `reason` == ActiveWebStateChangeReason::Activated.
      break;
    case WebStateListChange::Type::kDetach: {
      if (!web_state_list_->empty()) {
        return;
      }

      // Persist the session state after CloseAllWebStates. SaveSession will
      // discard calls when the web_state_list is not empty and the active
      // WebState is null, which is the order CloseAllWebStates uses.
      SaveSession(/*immediately=*/false);
      break;
    }
    case WebStateListChange::Type::kMove: {
      const WebStateListChangeMove& move_change =
          change.As<WebStateListChangeMove>();
      if (move_change.moved_web_state()->IsLoading()) {
        return;
      }

      // Persist the session state if the new web state is not loading.
      SaveSession(/*immediately=*/false);
      break;
    }
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace_change =
          change.As<WebStateListChangeReplace>();
      if (replace_change.inserted_web_state()->IsLoading()) {
        return;
      }

      // Persist the session state if the new web state is not loading.
      SaveSession(/*immediately=*/false);
      break;
    }
    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insert_change =
          change.As<WebStateListChangeInsert>();
      if (selection.activating ||
          insert_change.inserted_web_state()->IsLoading()) {
        return;
      }

      // Persist the session state if the new web state is not loading.
      SaveSession(/*immediately=*/false);
      break;
    }
  }
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

void SessionRestorationBrowserAgent::WillBeginBatchOperation(
    WebStateList* web_state_list) {
  batch_in_progress_ = true;
  save_after_batch_ = false;
  save_immediately_ = false;
}

void SessionRestorationBrowserAgent::BatchOperationEnded(
    WebStateList* web_state_list) {
  batch_in_progress_ = false;
  if (save_after_batch_) {
    SaveSession(save_immediately_);
    save_after_batch_ = false;
    save_immediately_ = false;
  }
}

#pragma mark - WebStateObserver

void SessionRestorationBrowserAgent::DidFinishNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  // Save the session each time a navigation finishes.
  SaveSession(/*immediately=*/false);
}
