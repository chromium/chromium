// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_TAB_HELPER_H_

#import <UIKit/UIKit.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "components/infobars/core/infobar_manager.h"
#include "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@class SnapshotGenerator;
@protocol SnapshotGeneratorDelegate;

namespace web {
class WebState;
}

// SnapshotTabHelper allows capturing and retrival for web page snapshots.
class SnapshotTabHelper : public infobars::InfoBarManager::Observer,
                          public web::WebStateObserver,
                          public web::WebStateUserData<SnapshotTabHelper> {
 public:
  ~SnapshotTabHelper() override;

  // Creates the tab helper for |web_state| if it does not exists. The
  // unique identifier |session_id| is used when interacting with the
  // cache to save or fetch snapshots.
  static void CreateForWebState(web::WebState* web_state, NSString* session_id);

  // Sets the delegate. Capturing snapshot before setting a delegate will
  // results in failures. The delegate is not owned by the tab helper.
  void SetDelegate(id<SnapshotGeneratorDelegate> delegate);

  // Retrieves a color snapshot for the current page, invoking |callback|
  // with the image. The callback may be called synchronously is there is
  // a cached snapshot available in memory, otherwise it will be invoked
  // asynchronously after retrieved from disk. Invokes |callback| with nil if a
  // snapshot does not exist.
  void RetrieveColorSnapshot(void (^callback)(UIImage*));

  // Retrieves a grey snapshot for the current page, invoking |callback|
  // with the image. The callback may be called synchronously is there is
  // a cached snapshot available in memory, otherwise it will be invoked
  // asynchronously after retrieved from disk or re-generated. Invokes
  // |callback| with nil if a snapshot does not exist.
  void RetrieveGreySnapshot(void (^callback)(UIImage*));

  // Asynchronously generates a new snapshot, updates the snapshot cache, and
  // invokes |callback| with the new snapshot image. Invokes |callback| with nil
  // if snapshot generation fails.
  void UpdateSnapshotWithCallback(void (^callback)(UIImage*));

  // DEPRECATED(crbug.com/917929): Use the asynchronous function
  // |UpdateSnapshotWithCallback()| for all new callsites.
  // Generates a new snapshot, updates the snapshot cache, and returns the new
  // snapshot image. Returns nil if snapshot generation fails.
  UIImage* UpdateSnapshot();

  // Generates a new snapshot without any overlays, and returns the new snapshot
  // image. This does not update the snapshot cache. Returns nil if snapshot
  // generation fails.
  UIImage* GenerateSnapshotWithoutOverlays();

  // Requests deletion of the current page snapshot from disk and memory.
  void RemoveSnapshot();

  // Instructs the helper not to snapshot content for the next page load event.
  void IgnoreNextLoad();

 private:
  friend class web::WebStateUserData<SnapshotTabHelper>;

  SnapshotTabHelper(web::WebState* web_state, NSString* session_id);

  // web::WebStateObserver implementation.
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  // infobars::InfoBarManager::Observer implementation.
  void OnInfoBarAdded(infobars::InfoBar* infobar) override;
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;
  void OnInfoBarReplaced(infobars::InfoBar* old_infobar,
                         infobars::InfoBar* new_infobar) override;
  void OnManagerShuttingDown(infobars::InfoBarManager* manager) override;

  web::WebState* web_state_ = nullptr;
  SnapshotGenerator* snapshot_generator_ = nil;
  infobars::InfoBarManager* infobar_manager_ = nullptr;

  // Manages this object as an observer of |web_state_|.
  ScopedObserver<web::WebState, web::WebStateObserver> web_state_observer_;

  // Manages this object as an observer of infobars.
  ScopedObserver<infobars::InfoBarManager, infobars::InfoBarManager::Observer>
      infobar_observer_;

  bool ignore_next_load_ = false;

  // True if |web_state_| was loading at the time of the last call to
  // UpdateSnapshotWithCallback(). If true, the last snapshot is expected to
  // become stale once the new page is loaded and rendered.
  bool was_loading_during_last_snapshot_ = false;

  // Used to ensure |UpdateSnapshotWithCallback()| is not run when this object
  // is destroyed.
  base::WeakPtrFactory<SnapshotTabHelper> weak_ptr_factory_;

  WEB_STATE_USER_DATA_KEY_DECL();

  DISALLOW_COPY_AND_ASSIGN(SnapshotTabHelper);
};

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_TAB_HELPER_H_
