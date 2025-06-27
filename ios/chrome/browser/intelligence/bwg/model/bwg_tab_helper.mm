// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/model/bwg_tab_helper.h"

#import "ios/chrome/browser/shared/public/commands/bwg_commands.h"
#import "ios/web/public/web_state.h"

BwgTabHelper::BwgTabHelper(web::WebState* web_state) {
  web_state_observation_.Observe(web_state);
}

BwgTabHelper::~BwgTabHelper() {}

void BwgTabHelper::SetBwgSessionActive(bool active) {
  is_bwg_session_active_ = active;
}

void BwgTabHelper::SetBwgCommandsHandler(id<BWGCommands> handler) {
  bwg_commands_handler_ = handler;
}

#pragma mark - WebStateObserver

void BwgTabHelper::WasShown(web::WebState* web_state) {
  if (is_bwg_session_active_) {
    [bwg_commands_handler_ startBWGFlow];
  }
}

void BwgTabHelper::WasHidden(web::WebState* web_state) {
  if (is_bwg_session_active_) {
    [bwg_commands_handler_ dismissBWGFlow];
  }
}
