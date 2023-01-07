// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"

#import "base/bind.h"
#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_macros.h"
#import "base/threading/sequenced_task_runner_handle.h"
#import "ios/chrome/browser/snapshots/snapshot_cache.h"
#import "ios/chrome/browser/snapshots/snapshot_generator.h"
#import "ios/web/public/thread/web_task_traits.h"
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

void SnapshotTabHelper::SetDelegate(id<SnapshotGeneratorDelegate> delegate) {
  snapshot_generator_.delegate = delegate;
}

void SnapshotTabHelper::SetSnapshotCache(SnapshotCache* snapshot_cache) {
  snapshot_generator_.snapshotCache = snapshot_cache;
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
  UIImage* image = [snapshot_generator_ updateSnapshot];
  dispatch_async(dispatch_get_main_queue(), ^{
    if (callback)
      callback(image);
  });
}

UIImage* SnapshotTabHelper::GenerateSnapshotWithoutOverlays() {
  return [snapshot_generator_ generateSnapshotWithOverlays:NO];
}

void SnapshotTabHelper::RemoveSnapshot() {
  [snapshot_generator_ removeSnapshot];
}

void SnapshotTabHelper::IgnoreNextLoad() {
  ignore_next_load_ = true;
}

void SnapshotTabHelper::WillBeSavedGreyWhenBackgrounding() {
  [snapshot_generator_.snapshotCache
      willBeSavedGreyWhenBackgrounding:web_state_->GetStableIdentifier()];
}

void SnapshotTabHelper::SaveGreyInBackground() {
  [snapshot_generator_.snapshotCache
      saveGreyInBackgroundForSnapshotID:web_state_->GetStableIdentifier()];
}

SnapshotTabHelper::SnapshotTabHelper(web::WebState* web_state)
    : web_state_(web_state), weak_ptr_factory_(this) {
  DCHECK(web_state_);
  DCHECK(web_state_->GetStableIdentifier().length > 0);
  snapshot_generator_ = [[SnapshotGenerator alloc]
      initWithWebState:web_state_
                 tabID:web_state_->GetStableIdentifier()];
  web_state_observation_.Observe(web_state_);
}

void SnapshotTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  // Snapshots taken while page is loading will eventually be stale. It
  // is important that another snapshot is taken after the new
  // page has loaded to replace the stale snapshot. The
  // `IOS.PageLoadedSnapshotResult` histogram shows the outcome of
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
      base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
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
          base::Seconds(1));
      break;
  }
  ignore_next_load_ = false;
  was_loading_during_last_snapshot_ = false;
}

void SnapshotTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state);
  DCHECK(web_state_observation_.IsObservingSource(web_state));
  web_state_observation_.Reset();
  web_state_ = nullptr;
}

WEB_STATE_USER_DATA_KEY_IMPL(SnapshotTabHelper)
