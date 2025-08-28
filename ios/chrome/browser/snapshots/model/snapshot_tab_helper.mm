// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/model/snapshot_tab_helper.h"

#import "base/functional/bind.h"
#import "base/memory/ptr_util.h"
#import "base/metrics/histogram_functions.h"
#import "base/task/sequenced_task_runner.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/snapshots/model/features.h"
#import "ios/chrome/browser/snapshots/model/legacy_snapshot_generator.h"
#import "ios/chrome/browser/snapshots/model/legacy_snapshot_manager.h"
#import "ios/chrome/browser/snapshots/model/model_swift.h"
#import "ios/chrome/browser/snapshots/model/snapshot_id.h"
#import "ios/chrome/browser/snapshots/model/snapshot_id_wrapper.h"
#import "ios/chrome/browser/snapshots/model/snapshot_util.h"
#import "ios/chrome/browser/snapshots/model/web_state_snapshot_info.h"
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

}  // namespace

SnapshotTabHelper::~SnapshotTabHelper() {
  DCHECK(!web_state_);
}

void SnapshotTabHelper::SetDelegate(id<SnapshotGeneratorDelegate> delegate) {
  CHECK(snapshot_manager_);
  [snapshot_manager_ setDelegate:delegate];
}

void SnapshotTabHelper::SetSnapshotStorage(id<SnapshotStorage> storage) {
  CHECK(snapshot_manager_);
  [snapshot_manager_ setStorage:storage];
}

void SnapshotTabHelper::RetrieveColorSnapshot(SnapshotRetrievedBlock callback) {
  CHECK(snapshot_manager_);
  [snapshot_manager_
      retrieveSnaphotWithKind:SnapshotKindColor
                   completion:BlockRecordingElapsedTime(
                                  SnapshotOperation::kRetrieveColorSnapshot,
                                  callback)];
}

void SnapshotTabHelper::RetrieveGreySnapshot(SnapshotRetrievedBlock callback) {
  CHECK(snapshot_manager_);
  CHECK(!base::FeatureList::IsEnabled(kRemoveGreySnapshot));
  [snapshot_manager_
      retrieveSnaphotWithKind:SnapshotKindGreyscale
                   completion:BlockRecordingElapsedTime(
                                  SnapshotOperation::kRetrieveGreyscaleSnapshot,
                                  callback)];
}

void SnapshotTabHelper::UpdateSnapshotWithCallback(
    SnapshotRetrievedBlock callback) {
  CHECK(snapshot_manager_);
  was_loading_during_last_snapshot_ = web_state_->IsLoading();
  [snapshot_manager_
      updateSnapshotWithCompletion:BlockRecordingElapsedTime(
                                       SnapshotOperation::kUpdateSnapshot,
                                       callback)];
}

void SnapshotTabHelper::UpdateSnapshotStorageWithImage(UIImage* image) {
  CHECK(snapshot_manager_);
  [snapshot_manager_ updateSnapshotStorageWithImage:image];
}

UIImage* SnapshotTabHelper::GenerateSnapshotWithoutOverlays() {
  CHECK(snapshot_manager_);
  return [snapshot_manager_ generateUIViewSnapshot];
}

void SnapshotTabHelper::IgnoreNextLoad() {
  ignore_next_load_ = true;
}

SnapshotTabHelper::SnapshotTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  DCHECK(web_state_);
  const SnapshotID snapshot_id(web_state_->GetUniqueIdentifier());
  if (base::FeatureList::IsEnabled(kSnapshotInSwift)) {
    snapshot_manager_ = [[SnapshotManagerImpl alloc]
        initWithGenerator:
            [[SnapshotGenerator alloc]
                initWithWebStateInfo:[[WebStateSnapshotInfo alloc]
                                         initWithWebState:web_state_]]
               snapshotID:[[SnapshotIDWrapper alloc]
                              initWithSnapshotID:snapshot_id]];
  } else {
    snapshot_manager_ = [[LegacySnapshotManager alloc]
        initWithGenerator:[[LegacySnapshotGenerator alloc]
                              initWithWebState:web_state_]
               snapshotID:snapshot_id];
  }

  web_state_observation_.Observe(web_state_.get());
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
      // Do not take a snapshot after loading chrome://newtab since there are no
      // page contents, just UIView. This prevents taking a snapshot while the
      // user is in the tabswitcher and thus the NTP is not in the view
      // hierarchy. In this case, the safe area insets are not available, so the
      // snapshot will cut off view contents.
      if (ignore_next_load_ || IsVisibleURLNewTabPage(web_state)) {
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
