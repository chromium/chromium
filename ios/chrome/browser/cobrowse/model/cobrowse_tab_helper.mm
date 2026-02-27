// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/cobrowse/model/cobrowse_tab_helper.h"

#import "ios/chrome/browser/shared/public/commands/scene_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/web/public/navigation/navigation_context.h"
#import "ios/web/public/web_state.h"

CobrowseTabHelper::CobrowseTabHelper(web::WebState* web_state) {
  CHECK(IsAimCobrowseEnabled());
  observation_.Observe(web_state);
}

CobrowseTabHelper::~CobrowseTabHelper() = default;

#pragma mark - Public

void CobrowseTabHelper::SetSceneCommandsHandler(id<SceneCommands> handler) {
  scene_commands_handler_ = handler;
}

void CobrowseTabHelper::SetDelegate(Delegate* delegate) {
  delegate_ = delegate;
}

#pragma mark - WebStateObserver

void CobrowseTabHelper::DidStartNavigation(
    web::WebState* web_state,
    web::NavigationContext* navigation_context) {
  if (!delegate_ || !scene_commands_handler_) {
    return;
  }

  if (delegate_->CanShowAssistantForWebState(web_state)) {
    [scene_commands_handler_ showAssistant];
  }
}

void CobrowseTabHelper::WebStateDestroyed(web::WebState* web_state) {
  observation_.Reset();
}
