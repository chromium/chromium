// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"

#import "base/functional/bind.h"
#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/snapshots/model/snapshot_generator.h"
#import "ios/chrome/browser/snapshots/model/snapshot_manager.h"
#import "ios/chrome/browser/snapshots/model/snapshot_storage.h"
#import "ios/web/public/web_client.h"
#import "ios/web/public/web_state.h"

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

// Generates an ID for WebState's snapshot.
SnapshotID GenerateSnapshotID(const web::WebState* web_state) {
  DCHECK(web_state->GetUniqueIdentifier().valid());
  DCHECK_GT(web_state->GetUniqueIdentifier().identifier(), 0);

  static_assert(sizeof(decltype(web::WebStateID().identifier())) ==
                sizeof(int32_t));
  return SnapshotID(web_state->GetUniqueIdentifier().identifier());
}

}  // namespace

SnapshotTabHelper::~SnapshotTabHelper() {
  DCHECK(!web_state_);
}

void SnapshotTabHelper::SetDelegate(id<SnapshotGeneratorDelegate> delegate) {
  [snapshot_manager_ setDelegate:delegate];
}

void SnapshotTabHelper::SetSnapshotStorage(SnapshotStorage* snapshot_storage) {
  snapshot_manager_.snapshotStorage = snapshot_storage;
}

void SnapshotTabHelper::RetrieveColorSnapshot(void (^callback)(UIImage*)) {
  [snapshot_manager_ retrieveSnapshot:callback];
}

void SnapshotTabHelper::RetrieveGreySnapshot(void (^callback)(UIImage*)) {
  [snapshot_manager_ retrieveGreySnapshot:callback];
}

void SnapshotTabHelper::UpdateSnapshotWithCallback(void (^callback)(UIImage*)) {
  was_loading_during_last_snapshot_ = web_state_->IsLoading();

  bool showing_native_content =
      web::GetWebClient()->IsAppSpecificURL(web_state_->GetLastCommittedURL());
  if (!showing_native_content && web_state_->CanTakeSnapshot()) {
    // Take the snapshot using the optimized WKWebView snapshotting API for
    // pages loaded in the web view when the WebState snapshot API is available.
    [snapshot_manager_ updateWKWebViewSnapshotWithCompletion:callback];
    return;
  }
  // Use the UIKit-based snapshot API as a fallback when the WKWebView API is
  // unavailable.
  [snapshot_manager_ updateUIViewSnapshotWithCompletion:callback];
}

UIImage* SnapshotTabHelper::GenerateSnapshotWithoutOverlays() {
  return [snapshot_manager_ generateUIViewSnapshot];
}

void SnapshotTabHelper::RemoveSnapshot() {
  [snapshot_manager_ removeSnapshot];
}

void SnapshotTabHelper::IgnoreNextLoad() {
  ignore_next_load_ = true;
}

void SnapshotTabHelper::WillBeSavedGreyWhenBackgrounding() {
  [snapshot_manager_ willBeSavedGreyWhenBackgrounding];
}

void SnapshotTabHelper::SaveGreyInBackground() {
  [snapshot_manager_ saveGreyInBackground];
}

SnapshotID SnapshotTabHelper::GetSnapshotID() const {
  return snapshot_manager_.snapshotID;
}

SnapshotTabHelper::SnapshotTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  DCHECK(web_state_);
  snapshot_manager_ = [[SnapshotManager alloc]
      initWithGenerator:[[SnapshotGenerator alloc] initWithWebState:web_state_]
             snapshotID:GenerateSnapshotID(web_state_)];
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
        base::UmaHistogramEnumeration(
            "IOS.PageLoadedSnapshotResult",
            PageLoadedSnapshotResult::
                kSnapshotNotAttemptedBecausePageLoadFailed);
      }
      break;

    case web::PageLoadCompletionStatus::SUCCESS:
      if (ignore_next_load_) {
        break;
      }

      bool was_loading = was_loading_during_last_snapshot_;
      base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(
              &SnapshotTabHelper::UpdateSnapshotWithCallback,
              weak_ptr_factory_.GetWeakPtr(),
              ^(UIImage* snapshot) {
                // Only log histogram for when a stale snapshot needs to be
                // replaced.
                if (!was_loading) {
                  return;
                }
                base::UmaHistogramEnumeration(
                    "IOS.PageLoadedSnapshotResult",
                    snapshot ? PageLoadedSnapshotResult::kSnapshotSucceeded
                             : PageLoadedSnapshotResult::
                                   kSnapshotAttemptedAndFailed);
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
