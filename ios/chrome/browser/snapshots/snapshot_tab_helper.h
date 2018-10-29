// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_TAB_HELPER_H_

#include <memory>

#import <UIKit/UIKit.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/infobars/core/infobar_manager.h"
#import "ios/web/public/web_state/web_state_observer.h"
#import "ios/web/public/web_state/web_state_user_data.h"

@class SnapshotGenerator;
@protocol SnapshotGeneratorDelegate;

// SnapshotTabHelper allows capturing and retrival for web page snapshots.
class SnapshotTabHelper : public web::WebStateObserver,
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

  // Returns the size the snapshot for the current page would have if it
  // was regenerated. If capturing the snapshot is not possible, returns
  // CGSizeZero.
  CGSize GetSnapshotSize() const;

  // Retrieves a color snapshot for the current page, invoking |callback|
  // with the image. The callback may be called synchronously is there is
  // a cached snapshot available in memory, otherwise it will be invoked
  // asynchronously after retrieved from disk or re-generated.
  void RetrieveColorSnapshot(void (^callback)(UIImage*));

  // Retrieves a grey snapshot for the current page, invoking |callback|
  // with the image. The callback may be called synchronously is there is
  // a cached snapshot available in memory, otherwise it will be invoked
  // asynchronously after retrieved from disk or re-generated.
  void RetrieveGreySnapshot(void (^callback)(UIImage*));

  // Invalidates the cached snapshot for the current page and forces the
  // generation of a more recent snapshot. Runs |callback| with a snapshot with
  // overlaid views (e.g. infobar) of the visible frame. In iOS 11+, snapshots
  // of a valid web view are taken using WKWebView's snapshotting API.
  void UpdateSnapshotWithCallback(void (^callback)(UIImage*));

  // Invalidates the cached snapshot for the current page and forces the
  // generation of a more recent snapshot. Returns the snapshot with or
  // without the overlaid views (e.g. infobar) and either of the visible frame
  // or of the full screen.
  UIImage* UpdateSnapshot(bool with_overlays, bool visible_frame_only);

  // Generates a snapshot for the current page. Returns the snapshot with
  // or without the overlaid views (e.g. infobar) and either of the visible
  // frame of of the full screen.
  UIImage* GenerateSnapshot(bool with_overlays, bool visible_frame_only);

  // When snapshot coalescing is enabled, multiple calls to generate a
  // snapshot with the same parameters may be coalesced.
  void SetSnapshotCoalescingEnabled(bool enabled);

  // Requests deletion of the current page snapshot from disk and memory.
  void RemoveSnapshot();

  // Instructs the helper not to snapshot content for the next page load event.
  void IgnoreNextLoad();

  // Instructs the helper to pause taking snapshots.
  void PauseSnapshotting();

  // Instructs the helper to resume taking snapshots.
  void ResumeSnapshotting();

  // Returns an image to use as replacement of a missing snapshot.
  static UIImage* GetDefaultSnapshotImage();

 private:
  SnapshotTabHelper(web::WebState* web_state, NSString* session_id);

  // web::WebStateObserver implementation.
  void DidStartLoading(web::WebState* web_state) override;
  void PageLoaded(
      web::WebState* web_state,
      web::PageLoadCompletionStatus load_completion_status) override;
  void WebStateDestroyed(web::WebState* web_state) override;

  web::WebState* web_state_ = nullptr;
  SnapshotGenerator* snapshot_generator_ = nil;
  std::unique_ptr<infobars::InfoBarManager::Observer> infobar_observer_;
  bool ignore_next_load_ = false;
  bool pause_snapshotting_ = false;

  // Used to ensure |UpdateSnapshotWithCallback()| is not run when this object
  // is destroyed.
  base::WeakPtrFactory<SnapshotTabHelper> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SnapshotTabHelper);
};

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_SNAPSHOT_TAB_HELPER_H_
