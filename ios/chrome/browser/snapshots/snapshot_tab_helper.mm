// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/snapshots/snapshot_tab_helper.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/post_task.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"
#include "ios/chrome/browser/infobars/infobar_manager_impl.h"
#import "ios/chrome/browser/snapshots/snapshot_generator.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#include "ios/web/public/web_task_traits.h"
#include "ios/web/public/web_thread.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

DEFINE_WEB_STATE_USER_DATA_KEY(SnapshotTabHelper);

namespace {

// SnapshotInfobarObserver watches an InfobarManager for InfoBar insertions,
// removals or replacements and updates the page snapshot in response.
class SnapshotInfobarObserver : public infobars::InfoBarManager::Observer {
 public:
  SnapshotInfobarObserver(SnapshotTabHelper* owner,
                          infobars::InfoBarManager* manager);
  ~SnapshotInfobarObserver() override;

  // infobars::InfoBarManager::Observer implementation.
  void OnInfoBarAdded(infobars::InfoBar* infobar) override;
  void OnInfoBarRemoved(infobars::InfoBar* infobar, bool animate) override;
  void OnInfoBarReplaced(infobars::InfoBar* old_infobar,
                         infobars::InfoBar* new_infobar) override;
  void OnManagerShuttingDown(infobars::InfoBarManager* manager) override;

 private:
  void OnInfoBarChanges();

  SnapshotTabHelper* owner_ = nullptr;
  infobars::InfoBarManager* manager_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(SnapshotInfobarObserver);
};

SnapshotInfobarObserver::SnapshotInfobarObserver(
    SnapshotTabHelper* owner,
    infobars::InfoBarManager* manager)
    : owner_(owner), manager_(manager) {
  DCHECK(owner_);
  DCHECK(manager_);
  manager_->AddObserver(this);
}

SnapshotInfobarObserver::~SnapshotInfobarObserver() {
  if (manager_) {
    manager_->RemoveObserver(this);
    manager_ = nullptr;
  }
}

void SnapshotInfobarObserver::OnInfoBarAdded(infobars::InfoBar* infobar) {
  OnInfoBarChanges();
}

void SnapshotInfobarObserver::OnInfoBarRemoved(infobars::InfoBar* infobar,
                                               bool animate) {
  OnInfoBarChanges();
}

void SnapshotInfobarObserver::OnInfoBarReplaced(
    infobars::InfoBar* old_infobar,
    infobars::InfoBar* new_infobar) {
  OnInfoBarChanges();
}

void SnapshotInfobarObserver::OnInfoBarChanges() {
  // Update the page snapshot on any infobar change.
  owner_->UpdateSnapshot(/*with_overlays=*/true,
                         /*visible_frame_only=*/true);
}

void SnapshotInfobarObserver::OnManagerShuttingDown(
    infobars::InfoBarManager* manager) {
  // The InfoBarManager delete itself when the WebState is destroyed, so
  // the observer needs to unregister itself when OnManagerShuttingDown
  // is invoked.
  DCHECK_EQ(manager_, manager);
  manager_->RemoveObserver(this);
  manager_ = nullptr;
}

}  // namespace;

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

CGSize SnapshotTabHelper::GetSnapshotSize() const {
  return [snapshot_generator_ snapshotSize];
}

void SnapshotTabHelper::RetrieveColorSnapshot(void (^callback)(UIImage*)) {
  [snapshot_generator_ retrieveSnapshot:callback];
}

void SnapshotTabHelper::RetrieveGreySnapshot(void (^callback)(UIImage*)) {
  [snapshot_generator_ retrieveGreySnapshot:callback];
}

void SnapshotTabHelper::UpdateSnapshotWithCallback(void (^callback)(UIImage*)) {
  if (IsWKWebViewSnapshotsEnabled() && web_state_->ContentIsHTML()) {
    if (@available(iOS 11, *)) {
      [snapshot_generator_ updateWebViewSnapshotWithCompletion:callback];
      return;
    }
  }
  // Pre-iOS 11 and native content cannot utilize the WKWebView snapshotting
  // API.
  UIImage* image =
      UpdateSnapshot(/*with_overlays=*/true, /*visible_frame_only=*/true);
  dispatch_async(dispatch_get_main_queue(), ^{
    if (callback)
      callback(image);
  });
}

UIImage* SnapshotTabHelper::UpdateSnapshot(bool with_overlays,
                                           bool visible_frame_only) {
  return [snapshot_generator_ updateSnapshotWithOverlays:with_overlays
                                        visibleFrameOnly:visible_frame_only];
}

UIImage* SnapshotTabHelper::GenerateSnapshot(bool with_overlays,
                                             bool visible_frame_only) {
  return [snapshot_generator_ generateSnapshotWithOverlays:with_overlays
                                          visibleFrameOnly:visible_frame_only];
}

void SnapshotTabHelper::SetSnapshotCoalescingEnabled(bool enabled) {
  [snapshot_generator_ setSnapshotCoalescingEnabled:enabled];
}

void SnapshotTabHelper::RemoveSnapshot() {
  DCHECK(web_state_);
  [snapshot_generator_ removeSnapshot];
}

void SnapshotTabHelper::IgnoreNextLoad() {
  ignore_next_load_ = true;
}

void SnapshotTabHelper::PauseSnapshotting() {
  pause_snapshotting_ = true;
}

void SnapshotTabHelper::ResumeSnapshotting() {
  pause_snapshotting_ = false;
}

// static
UIImage* SnapshotTabHelper::GetDefaultSnapshotImage() {
  return [SnapshotGenerator defaultSnapshotImage];
}

SnapshotTabHelper::SnapshotTabHelper(web::WebState* web_state,
                                     NSString* session_id)
    : web_state_(web_state), weak_ptr_factory_(this) {
  snapshot_generator_ = [[SnapshotGenerator alloc] initWithWebState:web_state_
                                                  snapshotSessionId:session_id];

  // Supports missing InfoBarManager to make testing easier.
  if (infobars::InfoBarManager* infobar_manager =
          InfoBarManagerImpl::FromWebState(web_state_)) {
    infobar_observer_ =
        std::make_unique<SnapshotInfobarObserver>(this, infobar_manager);
  }

  web_state_->AddObserver(this);
}

void SnapshotTabHelper::DidStartLoading(web::WebState* web_state) {
  if (IsWKWebViewSnapshotsEnabled())
    RemoveSnapshot();
}

void SnapshotTabHelper::PageLoaded(
    web::WebState* web_state,
    web::PageLoadCompletionStatus load_completion_status) {
  if (!ignore_next_load_ && !pause_snapshotting_ &&
      load_completion_status == web::PageLoadCompletionStatus::SUCCESS) {
    if (IsWKWebViewSnapshotsEnabled() && web_state->ContentIsHTML()) {
      if (@available(iOS 11, *)) {
        base::PostDelayedTaskWithTraits(
            FROM_HERE, {web::WebThread::UI},
            base::BindOnce(&SnapshotTabHelper::UpdateSnapshotWithCallback,
                           weak_ptr_factory_.GetWeakPtr(), /*callback=*/nil),
            base::TimeDelta::FromSeconds(1));
        return;
      }
    }
    // Pre-iOS 11 and native content cannot utilize the WKWebView snapshotting
    // API.
    UpdateSnapshot(/*with_overlays=*/true, /*visible_frame_only=*/true);
  }
  ignore_next_load_ = false;
}

void SnapshotTabHelper::WebStateDestroyed(web::WebState* web_state) {
  DCHECK_EQ(web_state_, web_state_);
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}
