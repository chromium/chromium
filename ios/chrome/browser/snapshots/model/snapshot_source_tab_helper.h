// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_SOURCE_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_SOURCE_TAB_HELPER_H_

#import <Foundation/Foundation.h>

#import "base/memory/raw_ptr.h"
#import "base/scoped_observation.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

namespace web {
class WebState;
}

@class UIImage;
@class UIView;

// A tab helper that provides snapshots for the associated tab. By default, it
// uses the WebState associated with this tab as the source for snapshots.
// However the source can be overridden with something else e.g. the Reader mode
// content in this tab, if there is any.
// TODO(crbug.com/441250817): Consider having better support for multiple
// WebStates in a single tab.
class SnapshotSourceTabHelper
    : public web::WebStateObserver,
      public web::WebStateUserData<SnapshotSourceTabHelper> {
 public:
  SnapshotSourceTabHelper(const SnapshotSourceTabHelper&) = delete;
  SnapshotSourceTabHelper& operator=(const SnapshotSourceTabHelper&) = delete;

  ~SnapshotSourceTabHelper() override;

  using SnapshotCallback = base::RepeatingCallback<void(UIImage*)>;

  // Returns whether TakeSnapshot() can be executed on the current snapshot
  // source.
  bool CanTakeSnapshot() const;

  // Takes a snapshot of the current snapshot source with `rect`. `rect` should
  // be specified in the coordinate system of the view returned by GetView().
  // `callback` is asynchronously invoked after performing the snapshot.
  void TakeSnapshot(const CGRect rect, SnapshotCallback callback);

  // The view containing the contents of the snapshot source.
  UIView* GetView();

  // Overrides the default snapshot source (`web_state_`) with another source.
  // If `overriding_source_web_state` is null then `web_state_` will be used.
  void SetOverridingSourceWebState(web::WebState* overriding_source_web_state);

 private:
  friend class web::WebStateUserData<SnapshotSourceTabHelper>;

  explicit SnapshotSourceTabHelper(web::WebState* web_state);

  // web::WebStateObserver implementation.
  void WebStateDestroyed(web::WebState* web_state) override;

  raw_ptr<web::WebState> web_state_ = nullptr;
  base::WeakPtr<web::WebState> overriding_source_web_state_;

  // Manages this object as an observer of `web_state_`.
  base::ScopedObservation<web::WebState, web::WebStateObserver>
      web_state_observation_{this};
};

#endif  // IOS_CHROME_BROWSER_SNAPSHOTS_MODEL_SNAPSHOT_SOURCE_TAB_HELPER_H_
