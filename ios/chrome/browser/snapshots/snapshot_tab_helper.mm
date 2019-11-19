// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/post_task.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/snapshots/snapshot_generator.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Possible results of snapshotting when the page has been loaded. These values
// are persisted to logs. Entries should not be renumbered and numeric values
// should never be reused.
enum class PageLoadedSnapshotResult {
  // Snapshot was not attempted, since the loading page will result in a stale
  // snapshot.
  kSnapshotNotAttemptedBecausePageLoadFailed = 0,
  // Snapshot was attempted, but the image is either the default image or nil.
  kSnapshotAttemptedAndFailed = 1,
  // Snapshot successfully taken.
  kSnapshotSucceeded = 2,
  // kMaxValue should share the value of the highest enumerator.
  kMaxValue = kSnapshotSucceeded,
};

}  // namespace

SnapshotTabHelper::~SnapshotTabHelper() {
  DCHECK(!web_state_);
}

// static
void SnapshotTabHelper::CreateForWebState(web::WebState* web_state,
                                          NSString* session_id) {
  DCHECK(web_state);
  if (!FromWebState(web_state)) {
    web_state->SetUserData(
        UserDataKey(),
        base::WrapUnique(new SnapshotTabHelper(web_state, session_id)));
  }
}

void SnapshotTabHelper::SetDelegate(id<SnapshotGeneratorDelegate> delegate) {
  snapshot_generator_.delegate = delegate;
}

void SnapshotTabHelper::RetrieveColorSnapshot(void (^callback)(UIImage*)) {
  [snapshot_generator_ retrieveSnapshot:callback];
}

void SnapshotTabHelper::RetrieveGreySnapshot(void (^callback)(UIImage*)) {
  [snapshot_generator_ retrieveGreySnapshot:callback];
}

void SnapshotTabHelper::UpdateSnapshotWithCallback(void (^callback)(UIImage*)) {
  was_loading_during_last_snapshot_ = web_state_->IsLoading();

  bool showing_native_content =
      web::GetWebClient()->IsAppSpecificURL(web_state_->GetLastCommittedURL());
  if (!showing_native_content && web_state_->CanTakeSnapshot()) {
    // Take the snapshot using the optimized WKWebView snapshotting API for
    // pages loaded in the web view when the WebState snapshot API is available.
    [snapshot_generator_ updateWebViewSnapshotWithCompletion:callback];
    return;
  }
  // Use the UIKit-based snapshot API as a fallback when the WKWebView API is
  // unavailable.
  UIImage* image = UpdateSnapshot();
  dispatch_async(dispatch_get_main_queue(), ^{
    if (callback)
      callback(image);
  });
}

UIImage* SnapshotTabHelper::UpdateSnapshot() {
  return [snapshot_generator_ updateSnapshot];
}

UIImage* SnapshotTabHelper::GenerateSnapshotWithoutOverlays() {
  return [snapshot_generator_ generateSnapshotWithOverlays:NO];
}

void SnapshotTabHelper::RemoveSnapshot() {
  DCHECK(web_state_);
  [snapshot_generator_ removeSnapshot];
}

void SnapshotTabHelper::IgnoreNextLoad() {
  ignore_next_load_ = true;
}

SnapshotTabHelper::SnapshotTabHelper(web::WebState* web_state,
                                     NSString* session_id)
    : web_state_(web_state),
      web_state_observer_(this),
      infobar_observer_(this),
      weak_ptr_factory_(this) {
  snapshot_generator_ = [[SnapshotGenerator alloc] initWithWebState:web_state_
                                                  snapshotSessionId:session_id];
  web_state_observer_.Add(web_state_);

  // Supports missing InfoBarManager to make testing easier.
  infobar_manager_ = InfoBarManagerImpl::FromWebState(web_state_);
  if (infobar_manager_) {
    infobar_observer_.Add(infobar_manager_);
  }
}

void SnapshotTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  // Snapshots taken while page is loading will eventually be stale. It
  // is important that another snapshot is taken after the new
  // page has loaded to replace the stale snapshot. The
  // |IOS.PageLoadedSnapshotResult| histogram shows the outcome of
  // snapshot attempts when the page is loaded after having taken
  // a stale snapshot.
  switch (load_completion_status) {
    case web::PageLoadCompletionStatus::FAILURE:
      // Only log histogram for when a stale snapshot needs to be replaced.
      if (was_loading_during_last_snapshot_) {
        UMA_HISTOGRAM_ENUMERATION(
            "IOS.PageLoadedSnapshotResult",
            PageLoadedSnapshotResult::
                kSnapshotNotAttemptedBecausePageLoadFailed);
      }
      break;

    case web::PageLoadCompletionStatus::SUCCESS:
      if (ignore_next_load_)
        break;

      bool was_loading = was_loading_during_last_snapshot_;
      base::PostDelayedTask(
          FROM_HERE, {web::WebThread::UI},
          base::BindOnce(
              &SnapshotTabHelper::UpdateSnapshotWithCallback,
              weak_ptr_factory_.GetWeakPtr(),
              ^(UIImage* snapshot) {
                // Only log histogram for when a stale snapshot needs to be
                // replaced.
                if (!was_loading)
                  return;
                PageLoadedSnapshotResult snapshotResult =
                    PageLoadedSnapshotResult::kSnapshotSucceeded;
                if (!snapshot) {
                  snapshotResult =
                      PageLoadedSnapshotResult::kSnapshotAttemptedAndFailed;
                }
                UMA_HISTOGRAM_ENUMERATION("IOS.PageLoadedSnapshotResult",
                                          snapshotResult);
              }),
          base::TimeDelta::FromSeconds(1));
      break;
  }
  ignore_next_load_ = false;
  was_loading_during_last_snapshot_ = false;
}

void SnapshotTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  web_state_observer_.Remove(web_state);
  web_state_ = nullptr;
}

void SnapshotTabHelper::OnInfoBarAdded(infobars::InfoBar* infobar) {
  UpdateSnapshotWithCallback(nil);
}

void SnapshotTabHelper::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                         bool animate) {
  UpdateSnapshotWithCallback(nil);
}

void SnapshotTabHelper::OnInfoBarReplaced(infobars::InfoBar* old_infobar,
                                          infobars::InfoBar* new_infobar) {
  UpdateSnapshotWithCallback(nil);
}

void SnapshotTabHelper::OnManagerShuttingDown(
    infobars::InfoBarManager* manager) {
  DCHECK_EQ(infobar_manager_, manager);
  infobar_observer_.Remove(manager);
  infobar_manager_ = nullptr;
}

WEB_STATE_USER_DATA_KEY_IMPL(SnapshotTabHelper)
