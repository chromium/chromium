// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_TAB_HELPER_H_
#define IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_TAB_HELPER_H_

#import "ios/web/public/web_state_observer.h"
#import "ios/web/public/web_state_user_data.h"

@protocol LensOverlayCommands;

// A tab helper that handles navigation to the tab with Lens Overlay
// by showing/hiding/killing the Lens Overlay UI.
class LensOverlayTabHelper : public web::WebStateUserData<LensOverlayTabHelper>,
                             public web::WebStateObserver {
 public:
  ~LensOverlayTabHelper() override;

  LensOverlayTabHelper(const LensOverlayTabHelper&) = delete;
  LensOverlayTabHelper& operator=(const LensOverlayTabHelper&) = delete;

  // Whether the lens overlay is displayed by the current tab helper.
  bool IsLensOverlayShown() { return is_showing_lens_overlay_; }

  // Whether the lens overlay is displayed by the current tab helper.
  void SetLensOverlayShown(bool is_showing_lens_overlay) {
    is_showing_lens_overlay_ = is_showing_lens_overlay;
  }

  // Sets the Lens Overlay commands handler.
  void SetLensOverlayCommandsHandler(id<LensOverlayCommands> commands_handler) {
    commands_handler_ = commands_handler;
  }

  // web::WebStateObserver:
  void WebStateDestroyed(web::WebState* web_state) override;
  void WasShown(web::WebState* web_state) override;
  void WasHidden(web::WebState* web_state) override;

 private:
  explicit LensOverlayTabHelper(web::WebState* web_state);

  // Handler used to request showing the password bottom sheet.
  __weak id<LensOverlayCommands> commands_handler_;

  // Tracks if lens overlay is displayed by the current tab helper
  bool is_showing_lens_overlay_ = false;

  // The WebState this instance is observing. Will be null after
  // WebStateDestroyed has been called.
  raw_ptr<web::WebState> web_state_ = nullptr;

  friend class web::WebStateUserData<LensOverlayTabHelper>;
  WEB_STATE_USER_DATA_KEY_DECL();
};

#endif  // IOS_CHROME_BROWSER_LENS_OVERLAY_MODEL_LENS_OVERLAY_TAB_HELPER_H_
