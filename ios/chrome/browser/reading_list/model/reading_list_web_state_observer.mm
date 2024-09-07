// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/reading_list/model/reading_list_web_state_observer.h"

#import <Foundation/Foundation.h>

#import "base/functional/bind.h"
#import "base/memory/ptr_util.h"
#import "base/memory/scoped_refptr.h"
#import "base/metrics/histogram_macros.h"
#import "components/reading_list/core/reading_list_model.h"
#import "ios/chrome/browser/reading_list/model/offline_url_utils.h"
#import "ios/chrome/browser/reading_list/model/reading_list_model_factory.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/web/public/navigation/navigation_item.h"
#import "ios/web/public/navigation/navigation_manager.h"
#import "ios/web/public/navigation/reload_type.h"
#import "ios/web/public/web_state_user_data.h"

ReadingListWebStateObserver::~ReadingListWebStateObserver() {
  if (reading_list_model_) {
    reading_list_model_->RemoveObserver(this);
    reading_list_model_ = nullptr;
  }
  // As the object can commit suicide, it is possible that its destructor
  // is called before WebStateDestroyed. In that case stop observing the
  // WebState.
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

ReadingListWebStateObserver::ReadingListWebStateObserver(
    web::WebState* web_state,
    ReadingListModel* reading_list_model)
    : web_state_(web_state),
      reading_list_model_(reading_list_model),
      last_load_was_offline_(false),
      last_load_result_(web::PageLoadCompletionStatus::SUCCESS) {
  web_state_->AddObserver(this);
  reading_list_model_->AddObserver(this);
}

bool ReadingListWebStateObserver::ShouldObserveItem(
    web::NavigationItem* item) const {
  if (!item) {
    return false;
  }

  return !reading_list::IsOfflineURL(item->GetURL());
}

bool ReadingListWebStateObserver::IsUrlAvailableOffline(const GURL& url) const {
  scoped_refptr<const ReadingListEntry> entry =
      reading_list_model_->GetEntryByURL(url);
  return entry && entry->DistilledState() == ReadingListEntry::PROCESSED;
}

void ReadingListWebStateObserver::ReadingListModelLoaded(
    const ReadingListModel* model) {
  DCHECK(model == reading_list_model_);
  if (web_state_->IsLoading()) {
    DidStartLoading(web_state_);
    return;
  }
  if (last_load_result_ == web::PageLoadCompletionStatus::SUCCESS) {
    return;
  }
  // An error page is being displayed.
  web::NavigationManager* manager = web_state_->GetNavigationManager();
  web::NavigationItem* item = manager->GetLastCommittedItem();
  if (!ShouldObserveItem(item)) {
    return;
  }
  const GURL& currentURL = item->GetVirtualURL();
  if (IsUrlAvailableOffline(currentURL)) {
    pending_url_ = currentURL;
    LoadOfflineReadingListEntry();
    StopCheckingProgress();
  }
}

void ReadingListWebStateObserver::ReadingListModelBeingDeleted(
    const ReadingListModel* model) {
  DCHECK_EQ(reading_list_model_, model);
  StopCheckingProgress();

  // The call to RemoveUserData cause the destruction of the current instance,
  // so nothing should be done after that point (this is like "delete this;").
  // Unregistration as an observer happens in the destructor.
  web_state_->RemoveUserData(UserDataKey());
}

void ReadingListWebStateObserver::DidStartLoading(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  StartCheckingLoading();
}

void ReadingListWebStateObserver::StartCheckingLoading() {
  DCHECK(reading_list_model_);
  DCHECK(web_state_);
  if (!reading_list_model_->loaded()) {
    StopCheckingProgress();
    return;
  }

  web::NavigationManager* manager = web_state_->GetNavigationManager();
  web::NavigationItem* item = manager->GetPendingItem();
  bool is_reload = false;

  // Manager->GetPendingItem() returns null on reload.
  // TODO(crbug.com/41292269): Remove this workaround once GetPendingItem()
  // returns the correct value on reload.
  if (!item) {
    item = manager->GetLastCommittedItem();
    is_reload = true;
  }

  if (!ShouldObserveItem(item)) {
    StopCheckingProgress();
    return;
  }
  bool last_load_was_offline = last_load_was_offline_;
  last_load_was_offline_ = false;

  pending_url_ = item->GetVirtualURL();

  is_reload =
      is_reload || ui::PageTransitionCoreTypeIs(item->GetTransitionType(),
                                                ui::PAGE_TRANSITION_RELOAD);
  // If the user is reloading from the offline page, the intention is to access
  // the online page even on bad networks. No need to launch timer.
  bool reloading_from_offline = last_load_was_offline && is_reload;

  // No need to launch the timer either if there is no offline version to show.
  // Track `pending_url_` to mark the entry as read in case of a successful
  // load.
  if (reloading_from_offline || !IsUrlAvailableOffline(pending_url_)) {
    return;
  }
  try_number_ = 0;
  timer_.reset(new base::RepeatingTimer());
  const base::TimeDelta kDelayUntilLoadingProgressIsChecked =
      base::Milliseconds(1500);
  timer_->Start(
      FROM_HERE, kDelayUntilLoadingProgressIsChecked,
      base::BindRepeating(
          &ReadingListWebStateObserver::VerifyIfReadingListEntryStartedLoading,
          weak_factory_.GetWeakPtr()));
}

void ReadingListWebStateObserver::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  DCHECK(reading_list_model_);
  DCHECK_EQ(web_state_, web_state);
  last_load_result_ = load_completion_status;
  web::NavigationItem* item =
      web_state_->GetNavigationManager()->GetLastCommittedItem();
  if (!item || !pending_url_.is_valid() ||
      !reading_list_model_->GetEntryByURL(pending_url_)) {
    StopCheckingProgress();
    return;
  }

  if (load_completion_status == web::PageLoadCompletionStatus::SUCCESS) {
    reading_list_model_->SetReadStatusIfExists(pending_url_, true);
    UMA_HISTOGRAM_BOOLEAN("ReadingList.OfflineVersionDisplayed", false);
  } else {
    LoadOfflineReadingListEntry();
  }
  StopCheckingProgress();
}

void ReadingListWebStateObserver::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  StopCheckingProgress();

  // The call to RemoveUserData cause the destruction of the current instance,
  // so nothing should be done after that point (this is like "delete this;").
  // Unregistration as an observer happens in the destructor.
  web_state_->RemoveUserData(UserDataKey());
}

void ReadingListWebStateObserver::StopCheckingProgress() {
  pending_url_ = GURL();
  timer_.reset();
}

void ReadingListWebStateObserver::VerifyIfReadingListEntryStartedLoading() {
  if (!pending_url_.is_valid()) {
    StopCheckingProgress();
    return;
  }
  web::NavigationManager* manager = web_state_->GetNavigationManager();
  web::NavigationItem* item = manager->GetPendingItem();

  // Manager->GetPendingItem() returns null on reload.
  // TODO(crbug.com/41292269): Remove this workaround once GetPendingItem()
  // returns the correct value on reload.
  if (!item) {
    item = manager->GetLastCommittedItem();
  }
  if (!item || !pending_url_.is_valid() ||
      !IsUrlAvailableOffline(pending_url_)) {
    StopCheckingProgress();
    return;
  }
  try_number_++;
  double progress = web_state_->GetLoadingProgress();
  const double kMinimumExpectedProgressPerStep = 0.25;
  if (progress < try_number_ * kMinimumExpectedProgressPerStep) {
    LoadOfflineReadingListEntry();
    StopCheckingProgress();
    return;
  }
  if (try_number_ >= 3) {
    // Loading reached 75%, let the page finish normal loading.
    // Do not call `StopCheckingProgress()` as `pending_url_` is still
    // needed to mark the entry read on success loading or to display
    // offline version on error.
    timer_->Stop();
  }
}

void ReadingListWebStateObserver::LoadOfflineReadingListEntry() {
  DCHECK(reading_list_model_);
  if (!pending_url_.is_valid() || !IsUrlAvailableOffline(pending_url_)) {
    return;
  }
  scoped_refptr<const ReadingListEntry> entry =
      reading_list_model_->GetEntryByURL(pending_url_);
  last_load_was_offline_ = true;
  DCHECK(entry->DistilledState() == ReadingListEntry::PROCESSED);
  GURL url = reading_list::OfflineURLForURL(entry->URL());
  web::NavigationManager* navigationManager =
      web_state_->GetNavigationManager();
  web::NavigationItem* item = navigationManager->GetPendingItem();
  if (!item) {
    // Either the loading finished on error and the item is already committed,
    // or the page is being reloaded and due to crbug.com/676129. there is no
    // pending item. Either way, the correct item to reuse is the last committed
    // item.
    // TODO(crbug.com/41292269): this case can be removed.
    item = navigationManager->GetLastCommittedItem();
    item->SetURL(url);
    item->SetVirtualURL(pending_url_);
    // Changing navigation item from web to native an calling
    // `navigationManager->Reload` will not insert NativeContent. This is a bug
    // which will not be fixed because NativeContent support is deprecated.
    // Offline Version will be eventually rewritten without relying on
    // NativeContent (crbug.com/725239).
    // Instead, go to the index that will branch further in the reload stack
    // and avoid this situation.
    navigationManager->GoToIndex(
        navigationManager->GetLastCommittedItemIndex());
  } else if (navigationManager->GetPendingItemIndex() != -1 &&
             navigationManager->GetItemAtIndex(
                 navigationManager->GetPendingItemIndex()) == item) {
    // The navigation corresponds to a back/forward. The item on the stack can
    // be reused for the offline navigation.
    // TODO(crbug.com/41286168): GetPendingItemIndex() will return
    // currentEntry() if navigating to a new URL. Test the addresses to verify
    // that GetPendingItemIndex() actually returns the pending item index.
    // Remove this extra test on item addresses once bug 665189 is fixed.
    item->SetURL(url);
    item->SetVirtualURL(pending_url_);
    navigationManager->GoToIndex(navigationManager->GetPendingItemIndex());
  } else {
    // The pending item corresponds to a new navigation and will be discarded
    // on next navigation.
    // Trigger a new navigation on the offline URL.
    web::WebState::OpenURLParams params(url, item->GetReferrer(),
                                        WindowOpenDisposition::CURRENT_TAB,
                                        item->GetTransitionType(), NO);
    web_state_->OpenURL(params);
  }
  reading_list_model_->SetReadStatusIfExists(entry->URL(), true);
  UMA_HISTOGRAM_BOOLEAN("ReadingList.OfflineVersionDisplayed", true);
}

WEB_STATE_USER_DATA_KEY_IMPL(ReadingListWebStateObserver)
