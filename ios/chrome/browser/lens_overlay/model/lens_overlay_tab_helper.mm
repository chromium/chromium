// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_tab_helper.h"

#import "base/check_op.h"
#import "ios/chrome/browser/lens_overlay/coordinator/lens_overlay_availability.h"
#import "ios/chrome/browser/shared/public/commands/lens_overlay_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"

LensOverlayTabHelper::LensOverlayTabHelper(web::WebState* web_state)
    : web_state_(web_state) {
  CHECK(IsLensOverlayAvailable());
  web_state->AddObserver(this);
}

LensOverlayTabHelper::~LensOverlayTabHelper() {
  if (web_state_) {
    web_state_->RemoveObserver(this);
    web_state_ = nullptr;
  }
}

#pragma mark - WebStateObserver

void LensOverlayTabHelper::WasShown(web::WebState* web_state) {
  CHECK_EQ(web_state, web_state_, kLensOverlayNotFatalUntil);

  if (is_showing_lens_overlay_) {
    [commands_handler_ showLensUI:YES];
  }
}

void LensOverlayTabHelper::WasHidden(web::WebState* web_state) {
  CHECK_EQ(web_state, web_state_, kLensOverlayNotFatalUntil);

  if (is_showing_lens_overlay_) {
    [commands_handler_ hideLensUI:YES];
  }
}

void LensOverlayTabHelper::WebStateDestroyed(web::WebState* web_state) {
  CHECK_EQ(web_state, web_state_, kLensOverlayNotFatalUntil);

  if (is_showing_lens_overlay_) {
    [commands_handler_ destroyLensUI:NO];
  }
  web_state_->RemoveObserver(this);
  web_state_ = nullptr;
}

WEB_STATE_USER_DATA_KEY_IMPL(LensOverlayTabHelper)
