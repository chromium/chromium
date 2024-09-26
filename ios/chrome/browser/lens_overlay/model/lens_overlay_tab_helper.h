// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_TAB_HELPER_H_

#import <UIKit/UIKit.h>

#include <memory>

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_snapshot_controller_delegate.h"
#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol LensOverlayCommands;
class LensOverlaySnapshotController;

// A tab helper that handles navigation to the tab with Lens Overlay
// by showing/hiding/killing the Lens Overlay UI.
class LensOverlayTabHelper : public LensOverlaySnapshotControllerDelegate,
                             public web::WebStateUserData<LensOverlayTabHelper>,
                             public web::WebStateObserver {
 public:
  ~LensOverlayTabHelper() override;

  LensOverlayTabHelper(const LensOverlayTabHelper&) = delete;
  LensOverlayTabHelper& operator=(const LensOverlayTabHelper&) = delete;

  using SnapshotCallback = base::OnceCallback<void(UIImage*)>;

  // Enters fullscreen and captures a new snapshot when the animation is
  // complete. If there is no snapshot controller set, then this callback will
  // be invoked with `nil`.
  void CaptureFullscreenSnapshot(SnapshotCallback);

  // Whether the lens overlay is displayed by the current tab helper.
  bool IsLensOverlayShown() { return is_showing_lens_overlay_; }

  // Whether the lens overlay is displayed by the current tab helper.
  void SetLensOverlayShown(bool is_showing_lens_overlay) {
    is_showing_lens_overlay_ = is_showing_lens_overlay;
  }

  // Whether there is an ongoing snapshot capture.
  bool IsCapturingLensOverlaySnapshot() {
    return is_capturing_lens_overlay_snapshot_;
  }

  bool IsUpdatingTabSwitcherSnapshot() {
    return is_updating_tab_switcher_snapshot_;
  }

  // Updates the lens overlay web state tab switcher snapshot.
  void UpdateSnapshot();

  // Sets the Lens Overlay commands handler.
  void SetLensOverlayCommandsHandler(id<LensOverlayCommands> commands_handler) {
    commands_handler_ = commands_handler;
  }

  // Sets the snapshot controller.
  void SetSnapshotController(
      std::unique_ptr<LensOverlaySnapshotController> snapshot_controller);

  // Returns the dimensions for the inset area of the lens overlay snapshot.
  UIEdgeInsets GetSnapshotInsets();

  // LensOverlaySnapshotControllerDelegate:
  void OnSnapshotCaptureBegin() override;
  void OnSnapshotCaptureEnd() override;

  web::WebState* GetWebState() const { return web_state_; }

  // web::WebStateObserver:
  void WebStateDestroyed(web::WebState* web_state) override;
  void WasShown(web::WebState* web_state) override;
  void WasHidden(web::WebState* web_state) override;
  void DidStartNavigation(web::WebState* web_state,
                          web::NavigationContext* navigation_context) override;

 private:
  explicit LensOverlayTabHelper(web::WebState* web_state);

  // Handler used to request showing the password bottom sheet.
  __weak id<LensOverlayCommands> commands_handler_;

  // Responsible for taking snapshots for the lens overlay
  std::unique_ptr<LensOverlaySnapshotController> snapshot_controller_;

  // Tracks if lens overlay is displayed by the current tab helper
  bool is_showing_lens_overlay_ = false;

  // Tracks whether there is a snapshot capture in progress.
  bool is_capturing_lens_overlay_snapshot_ = false;

  // Tracksa whether there is a tab switcher snapshot update in progress.
  bool is_updating_tab_switcher_snapshot_ = false;

  // The WebState this instance is observing. Will be null after
  // WebStateDestroyed has been called.
  raw_ptr<web::WebState> web_state_ = nullptr;

  base::WeakPtrFactory<LensOverlayTabHelper> weak_ptr_factory_{this};

  friend class web::WebStateUserData<LensOverlayTabHelper>;
  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_TAB_HELPER_H_
