// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_TAB_HELPER_H_

#import <UIKit/UIKit.h>

#import "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ios/chrome/browser/snapshots/model/snapshot_id.h"
#include "ios/web/public/web_state_observer.h"
#include "ios/web/public/web_state_user_data.h"

@class LegacySnapshotManager;
@class SnapshotManager;
@class SnapshotStorageWrapper;
@protocol SnapshotGeneratorDelegate;

namespace web {
class WebState;
}

// SnapshotTabHelper allows capturing and retrival for web page snapshots.
class SnapshotTabHelper : public web::WebStateObserver,
                          public web::WebStateUserData<SnapshotTabHelper> {
 public:
  SnapshotTabHelper(const SnapshotTabHelper&) = delete;
  SnapshotTabHelper& operator=(const SnapshotTabHelper&) = delete;

  ~SnapshotTabHelper() override;

  // Sets the delegate. Capturing snapshot before setting a delegate will
  // results in failures. The delegate is not owned by the tab helper.
  void SetDelegate(id<SnapshotGeneratorDelegate> delegate);

  // Sets the snapshot storage to be used to store and retrieve snapshots. This
  // is not owned by the tab helper.
  void SetSnapshotStorage(SnapshotStorageWrapper* wrapper);

  // Retrieves a color snapshot for the current page, invoking `callback` with
  // the image. The callback may be called synchronously if there is a cached
  // snapshot available in memory, otherwise it will be invoked asynchronously
  // after retrieved from disk. Invokes `callback` with nil if a snapshot does
  // not exist.
  void RetrieveColorSnapshot(void (^callback)(UIImage*));

  // Retrieves a grey snapshot for the current page, invoking `callback`
  // with the image. The callback may be called synchronously is there is
  // a cached snapshot available in memory, otherwise it will be invoked
  // asynchronously after retrieved from disk or re-generated. Invokes
  // `callback` with nil if a snapshot does not exist.
  void RetrieveGreySnapshot(void (^callback)(UIImage*));

  // Asynchronously generates a new snapshot, updates the snapshot storage, and
  // invokes `callback` with the new snapshot image. Invokes `callback` with nil
  // if snapshot generation fails.
  void UpdateSnapshotWithCallback(void (^callback)(UIImage*));

  // Generates a new snapshot without any overlays, and returns the new snapshot
  // image. This does not update the snapshot storage. Returns nil if snapshot
  // generation fails.
  UIImage* GenerateSnapshotWithoutOverlays();

  // Requests deletion of the current page snapshot from disk and memory.
  void RemoveSnapshot();

  // Instructs the helper not to snapshot content for the next page load event.
  void IgnoreNextLoad();

  // Returns the ID to use for the snapshot.
  SnapshotID GetSnapshotID() const;

 private:
  friend class web::WebStateUserData<SnapshotTabHelper>;

  explicit SnapshotTabHelper(web::WebState* web_state);

  // web::WebStateObserver implementation.
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  raw_ptr<web::WebState> web_state_ = nullptr;
  SnapshotManager* snapshot_manager_ = nil;
  LegacySnapshotManager* legacy_snapshot_manager_ = nil;

  // Manages this object as an observer of `web_state_`.
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};

  bool ignore_next_load_ = false;

  // True if `web_state_` was loading at the time of the last call to
  // UpdateSnapshotWithCallback(). If true, the last snapshot is expected to
  // become stale once the new page is loaded and rendered.
  bool was_loading_during_last_snapshot_ = false;

  // Used to ensure `UpdateSnapshotWithCallback()` is not run when this object
  // is destroyed.
  base::WeakPtrFactory<SnapshotTabHelper> weak_ptr_factory_{this};

  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_TAB_HELPER_H_
