// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/sessions/model/session_restoration_browser_agent.h"

#import <vector>

#import "base/apple/foundation_util.h"
#import "base/ios/ios_util.h"
#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/strings/sys_string_conversions.h"
#import "base/time/time.h"
#import "components/previous_session_info/previous_session_info.h"
#import "ios/chrome/browser/sessions/model/session_constants.h"
#import "ios/chrome/browser/sessions/model/session_restoration_observer.h"
#import "ios/chrome/browser/sessions/model/session_service_ios.h"
#import "ios/chrome/browser/sessions/model/session_tab_group.h"
#import "ios/chrome/browser/sessions/model/session_window_ios.h"
#import "ios/chrome/browser/sessions/model/session_window_ios_factory.h"
#import "ios/chrome/browser/sessions/model/web_session_state_cache.h"
#import "ios/chrome/browser/sessions/model/web_session_state_cache_factory.h"
#import "ios/chrome/browser/sessions/model/web_session_state_tab_helper.h"
#import "ios/chrome/browser/sessions/model/web_state_list_serialization.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/model/web_state_list/all_web_state_observation_forwarder.h"
#import "ios/chrome/browser/shared/model/web_state_list/order_controller.h"
#import "ios/chrome/browser/shared/model/web_state_list/order_controller_source.h"
#import "ios/chrome/browser/shared/model/web_state_list/removing_indexes.h"
#import "ios/chrome/browser/shared/model/web_state_list/tab_group_range.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/session/crw_session_storage.h"
#import "ios/web/public/session/crw_session_user_data.h"
#import "ios/web/public/web_state.h"

namespace {

// A concrete implementation of OrderControllerSource that query data
// from a SessionWindowIOS.
class OrderControllerSourceFromSessionWindowIOS final
    : public OrderControllerSource {
 public:
  // Constructor taking the `session_window` used to return the data.
  explicit OrderControllerSourceFromSessionWindowIOS(
      SessionWindowIOS* session_window);

  // OrderControllerSource implementation.
  int GetCount() const final;
  int GetPinnedCount() const final;
  int GetOpenerOfItemAt(int index) const final;
  bool IsOpenerOfItemAt(int index,
                        int opener_index,
                        bool check_navigation_index) const final;
  TabGroupRange GetGroupRangeOfItemAt(int index) const final;
  std::set<int> GetCollapsedGroupIndexes() const final;

 private:
  SessionWindowIOS* session_window_;
};

OrderControllerSourceFromSessionWindowIOS::
    OrderControllerSourceFromSessionWindowIOS(SessionWindowIOS* session_window)
    : session_window_(session_window) {}

int OrderControllerSourceFromSessionWindowIOS::GetCount() const {
  return static_cast<int>(session_window_.sessions.count);
}

int OrderControllerSourceFromSessionWindowIOS::GetPinnedCount() const {
  int pinned_count = 0;
  for (CRWSessionStorage* session in session_window_.sessions) {
    CRWSessionUserData* user_data = session.userData;
    NSNumber* pinned_obj = base::apple::ObjCCast<NSNumber>(
        [user_data objectForKey:kLegacyWebStateListPinnedStateKey]);

    // All pinned items are at the beginning of the list, so stop as
    // soon as the first unpinned tab is found.
    if (!pinned_obj || ![pinned_obj boolValue]) {
      break;
    }

    ++pinned_count;
  }
  return pinned_count;
}

int OrderControllerSourceFromSessionWindowIOS::GetOpenerOfItemAt(
    int index) const {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, GetCount());

  CRWSessionUserData* user_data = session_window_.sessions[index].userData;
  NSNumber* opener_index_obj = base::apple::ObjCCast<NSNumber>(
      [user_data objectForKey:kLegacyWebStateListOpenerIndexKey]);
  if (!opener_index_obj) {
    return WebStateList::kInvalidIndex;
  }

  return [opener_index_obj intValue];
}

bool OrderControllerSourceFromSessionWindowIOS::IsOpenerOfItemAt(
    int index,
    int opener_index,
    bool check_navigation_index) const {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, GetCount());

  // `check_navigation_index` is only used for `DetermineInsertionIndex()`
  // which should not be used, so we can assert that the parameter is false.
  DCHECK(!check_navigation_index);

  CRWSessionUserData* user_data = session_window_.sessions[index].userData;
  NSNumber* opener_index_obj = base::apple::ObjCCast<NSNumber>(
      [user_data objectForKey:kLegacyWebStateListOpenerIndexKey]);
  if (!opener_index_obj || [opener_index_obj intValue] != opener_index) {
    return false;
  }

  return true;
}

TabGroupRange OrderControllerSourceFromSessionWindowIOS::GetGroupRangeOfItemAt(
    int index) const {
  for (SessionTabGroup* group in session_window_.tabGroups) {
    const TabGroupRange group_range(group.rangeStart, group.rangeCount);
    if (group_range.contains(index)) {
      return group_range;
    }
  }
  return TabGroupRange::InvalidRange();
}

std::set<int>
OrderControllerSourceFromSessionWindowIOS::GetCollapsedGroupIndexes() const {
  std::set<int> collapsed_indexes;

  for (SessionTabGroup* group in session_window_.tabGroups) {
    if (group.collapsedState) {
      const TabGroupRange group_range(group.rangeStart, group.rangeCount);
      collapsed_indexes.insert(group_range.begin(), group_range.end());
    }
  }
  return collapsed_indexes;
}

// Determines the new active index.
NSUInteger GetActiveIndex(SessionWindowIOS* session_window,
                          const RemovingIndexes& removing_indexes) {
  int active_index = session_window.selectedIndex != NSNotFound
                         ? static_cast<int>(session_window.selectedIndex)
                         : WebStateList::kInvalidIndex;

  const OrderControllerSourceFromSessionWindowIOS source(session_window);
  const OrderController order_controller(source);

  // Update the `active_index` using the shared logic and the knowledge
  // of the removed items.
  active_index = removing_indexes.IndexAfterRemoval(
      order_controller.DetermineNewActiveIndex(active_index, removing_indexes));

  return active_index != WebStateList::kInvalidIndex
             ? static_cast<NSUInteger>(active_index)
             : NSNotFound;
}

// Updates opener_index for `session` according to `removing_indexes`.
void UpdateOpenerIndex(CRWSessionUserData* user_data,
                       const RemovingIndexes& removing_indexes) {
  NSNumber* opener_index_obj = base::apple::ObjCCast<NSNumber>(
      [user_data objectForKey:kLegacyWebStateListOpenerIndexKey]);
  if (!opener_index_obj) {
    return;
  }

  const int opener_index =
      removing_indexes.IndexAfterRemoval([opener_index_obj intValue]);
  if (opener_index == WebStateList::kInvalidIndex) {
    [user_data removeObjectForKey:kLegacyWebStateListOpenerIndexKey];
    [user_data removeObjectForKey:kLegacyWebStateListOpenerNavigationIndexKey];
  } else {
    [user_data setObject:@(opener_index)
                  forKey:kLegacyWebStateListOpenerIndexKey];
  }
}

// Filters out session items that are considered invalid: either because they
// are empty (no navigation), or duplicates.
SessionWindowIOS* FilterInvalidTabs(SessionWindowIOS* session_window) {
  DCHECK_LE(session_window.sessions.count, static_cast<NSUInteger>(INT_MAX));
  const int sessions_count = static_cast<int>(session_window.sessions.count);

  std::vector<int> items_to_drop;
  std::set<web::WebStateID> seen_identifiers;
  // Count the number of dropped tabs because they are duplicates, for
  // reporting.
  int duplicate_count = 0;
  for (int index = 0; index < sessions_count; ++index) {
    CRWSessionStorage* session = session_window.sessions[index];
    if (session.itemStorages.count == 0) {
      // Filter out session items that would be empty after restoration.
      items_to_drop.push_back(index);
    } else {
      // Filter out session items that are duplicate (after something went bad
      // somewhere).
      if (seen_identifiers.contains(session.uniqueIdentifier)) {
        items_to_drop.push_back(index);
        duplicate_count++;
      }
      seen_identifiers.insert(session.uniqueIdentifier);
    }
  }
  base::UmaHistogramCounts100("Tabs.DroppedDuplicatesCountOnSessionRestore",
                              duplicate_count);

  // Nothing to do.
  if (items_to_drop.empty()) {
    return session_window;
  }

  // Compute the new value of selectedIndex before updating the opener-opened
  // relationship, as OrderController take into account the closed WebStates.
  const RemovingIndexes removing_indexes(std::move(items_to_drop));
  const NSUInteger selected_index =
      GetActiveIndex(session_window, removing_indexes);

  // Create the new list of sessions, updating the opener-opened relationship
  // to take into account the dropped CRWSessionStorage items.
  NSMutableArray<CRWSessionStorage*>* sessions = [[NSMutableArray alloc] init];
  for (int index = 0; index < sessions_count; ++index) {
    if (removing_indexes.Contains(index)) {
      continue;
    }

    CRWSessionStorage* session = session_window.sessions[index];
    UpdateOpenerIndex(session.userData, removing_indexes);
    [sessions addObject:session];
  }

  // Create the new list of tab groups, updating the `rangeStart` and
  // `rangeCount` properties.
  NSMutableArray<SessionTabGroup*>* groups = [[NSMutableArray alloc] init];
  for (SessionTabGroup* group in session_window.tabGroups) {
    const TabGroupRange initial_range(group.rangeStart, group.rangeCount);
    const TabGroupRange final_range =
        removing_indexes.RangeAfterRemoval(initial_range);
    if (final_range.valid()) {
      group.rangeStart = final_range.range_begin();
      group.rangeCount = final_range.count();
      [groups addObject:group];
    }
  }

  return [[SessionWindowIOS alloc] initWithSessions:sessions
                                          tabGroups:groups
                                      selectedIndex:selected_index];
}

// Creates a WebState with `params` and `session_storage`.
std::unique_ptr<web::WebState> CreateWebState(
    const web::WebState::CreateParams& params,
    CRWSessionStorage* session_storage) {
  __weak WebSessionStateCache* weak_cache =
      WebSessionStateCacheFactory::GetForProfile(
          ProfileIOS::FromBrowserState(params.browser_state.get()));

  const web::WebStateID web_state_id = session_storage.uniqueIdentifier;
  return web::WebState::CreateWithStorageSession(
      params, session_storage, base::BindOnce(^{
        return [weak_cache sessionStateDataForWebStateID:web_state_id];
      }));
}

}  // namespace

BROWSER_USER_DATA_KEY_IMPL(SessionRestorationBrowserAgent)

SessionRestorationBrowserAgent::SessionRestorationBrowserAgent(
    Browser* browser,
    SessionServiceIOS* session_service,
    bool enable_pinned_web_states,
    bool enable_tab_groups)
    : session_service_(session_service),
      browser_(browser),
      session_window_ios_factory_([[SessionWindowIOSFactory alloc]
          initWithWebStateList:browser_->GetWebStateList()]),
      enable_pinned_web_states_(enable_pinned_web_states),
      enable_tab_groups_(enable_tab_groups),
      all_web_state_observer_(std::make_unique<AllWebStateObservationForwarder>(
          browser_->GetWebStateList(),
          this)) {
  browser_->AddObserver(this);
  browser_->GetWebStateList()->AddObserver(this);
}

SessionRestorationBrowserAgent::~SessionRestorationBrowserAgent() {
  // Disconnect the session factory object as it's not garanteed that it will
  // be released before it's referenced by the session service.
  [session_window_ios_factory_ disconnect];

  // If the object is destroyed before the Browser, unregister it from the
  // ObserverList explicitly.
  if (browser_) {
    BrowserDestroyed(browser_);
  }
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

void SessionRestorationBrowserAgent::RestoreSessionWindow(
    SessionWindowIOS* window) {
  // Start the session restoration.
  restoring_session_ = true;

  for (auto& observer : observers_) {
    observer.WillStartSessionRestoration(browser_);
  }

  // Restore the tabs (except the invalid ones).
  const std::vector<web::WebState*> restored_web_states =
      DeserializeWebStateList(
          browser_->GetWebStateList(), FilterInvalidTabs(window),
          enable_pinned_web_states_, enable_tab_groups_,
          base::BindRepeating(&CreateWebState, web::WebState::CreateParams(
                                                   browser_->GetProfile())));

  for (auto& observer : observers_) {
    observer.SessionRestorationFinished(browser_, restored_web_states);
  }

  // Session restoration is complete.
  restoring_session_ = false;

  // Schedule a session save.
  SaveSession(/*immediately*/ false);
}

void SessionRestorationBrowserAgent::RestoreSession() {
  DCHECK(session_identifier_.length != 0);

  const base::TimeTicks start_time = base::TimeTicks::Now();

  PreviousSessionInfo* session_info = [PreviousSessionInfo sharedInstance];
  base::ScopedClosureRunner scoped_restore =
      [session_info startSessionRestoration];

  SessionWindowIOS* session_window = [session_service_
      loadSessionWithSessionID:session_identifier_
                     directory:browser_->GetProfile()->GetStatePath()];

  RestoreSessionWindow(session_window);
  base::UmaHistogramTimes(kSessionHistogramLoadingTime,
                          base::TimeTicks::Now() - start_time);
}

bool SessionRestorationBrowserAgent::IsRestoringSession() {
  return restoring_session_;
}

void SessionRestorationBrowserAgent::SaveSession(bool immediately) {
  DCHECK(session_identifier_.length != 0);

  if (!CanSaveSession())
    return;

  WebStateList* const web_state_list = browser_->GetWebStateList();
  if (web_state_list->IsBatchInProgress()) {
    save_after_batch_ = true;
    save_immediately_ = save_immediately_ || immediately;
    return;
  }

  [session_service_ saveSession:session_window_ios_factory_
                      sessionID:session_identifier_
                      directory:browser_->GetProfile()->GetStatePath()
                    immediately:immediately];

  for (int i = 0; i < web_state_list->count(); ++i) {
    web::WebState* web_state = web_state_list->GetWebStateAt(i);
    if (WebSessionStateTabHelper* tab_helper =
            WebSessionStateTabHelper::FromWebState(web_state)) {
      tab_helper->SaveSessionStateIfStale();
    }
  }
}

bool SessionRestorationBrowserAgent::CanSaveSession() {
  // Do not schedule a save while a session restoration is in progress.
  if (restoring_session_) {
    return false;
  }

  // A session requires an active Browser.
  if (!browser_) {
    return false;
  }

  // Sessions where there's no active tab shouldn't be saved, unless the web
  // state list is empty. This is a transitional state.
  WebStateList* const web_state_list = browser_->GetWebStateList();
  if (!web_state_list->empty() && !web_state_list->GetActiveWebState()) {
    return false;
  }

  return true;
}

#pragma mark - BrowserObserver

void SessionRestorationBrowserAgent::BrowserDestroyed(Browser* browser) {
  DCHECK_EQ(browser, browser_);
  // Stop observing web states.
  all_web_state_observer_.reset();

  // Stop observing web state list.
  browser_->GetWebStateList()->RemoveObserver(this);
  browser_->RemoveObserver(this);
  browser_ = nullptr;
}

#pragma mark - WebStateListObserver

void SessionRestorationBrowserAgent::WebStateListWillChange(
    WebStateList* web_state_list,
    const WebStateListChangeDetach& detach_change,
    const WebStateListStatus& status) {
  DCHECK_EQ(browser_->GetWebStateList(), web_state_list);
  if (web_state_list->active_index() == detach_change.detached_from_index()) {
    return;
  }

  // Persist the session state if a background tab is detached.
  SaveSession(/*immediately=*/false);
}

void SessionRestorationBrowserAgent::WebStateListDidChange(
    WebStateList* web_state_list,
    const WebStateListChange& change,
    const WebStateListStatus& status) {
  DCHECK_EQ(browser_->GetWebStateList(), web_state_list);
  switch (change.type()) {
    case WebStateListChange::Type::kStatusOnly:
      // The activation is handled after this switch statement.
      break;
    case WebStateListChange::Type::kDetach: {
      if (!web_state_list->empty()) {
        break;
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
        break;
      }

      // Persist the session state if the new web state is not loading.
      SaveSession(/*immediately=*/false);
      break;
    }
    case WebStateListChange::Type::kReplace: {
      const WebStateListChangeReplace& replace_change =
          change.As<WebStateListChangeReplace>();
      if (replace_change.inserted_web_state()->IsLoading()) {
        break;
      }

      // Persist the session state if the new web state is not loading.
      SaveSession(/*immediately=*/false);
      break;
    }
    case WebStateListChange::Type::kInsert: {
      const WebStateListChangeInsert& insert_change =
          change.As<WebStateListChangeInsert>();
      if (status.active_web_state_change() ||
          insert_change.inserted_web_state()->IsLoading()) {
        break;
      }

      // Persist the session state if the new web state is not loading.
      SaveSession(/*immediately=*/false);
      break;
    }
    case WebStateListChange::Type::kGroupCreate:
      // Persist the session state.
      SaveSession(/*immediately=*/false);
      break;
    case WebStateListChange::Type::kGroupVisualDataUpdate:
      // Persist the session state.
      SaveSession(/*immediately=*/false);
      break;
    case WebStateListChange::Type::kGroupMove:
      // Persist the session state.
      SaveSession(/*immediately=*/false);
      break;
    case WebStateListChange::Type::kGroupDelete:
      // Persist the session state.
      SaveSession(/*immediately=*/false);
      break;
  }

  if (status.active_web_state_change()) {
    if (status.new_active_web_state &&
        status.new_active_web_state->IsLoading()) {
      return;
    }

    // Persist the session state if the new web state is not loading (or if
    // the last tab was closed).
    SaveSession(/*immediately=*/false);
  }
}

void SessionRestorationBrowserAgent::WillBeginBatchOperation(
    WebStateList* web_state_list) {
  save_after_batch_ = false;
  save_immediately_ = false;
}

void SessionRestorationBrowserAgent::BatchOperationEnded(
    WebStateList* web_state_list) {
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
