// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/app/task_request_for_remote_actor_url_context.h"

#import <optional>
#import <utility>

#import "base/ios/block_types.h"
#import "components/password_manager/core/common/remote_actor/remote_actor_features.h"
#import "ios/chrome/app/application_delegate/tab_opening.h"
#import "ios/chrome/app/startup/remote_actor_url_parser.h"
#import "ios/chrome/app/task_request_url_context_private.h"
#import "ios/chrome/browser/passwords/model/remote_actor/remote_actor_request_handler.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "net/base/apple/url_conversions.h"
#import "url/gurl.h"

@implementation TaskRequestForRemoteActorURLContext

// Requests are discarded if the feature is disabled or if the URL lacks the
// payload or nonce.
- (void)handleCommandWithSceneState:(SceneState*)sceneState {
  if (!password_manager::features::
          RemoteActorCredentialSharingGeminiEnabled()) {
    return;
  }
  std::optional<RemoteActorRequest> request =
      ExtractRemoteActorRequestFromURL(net::GURLWithNSURL(self.URLContext.URL));
  if (!request) {
    return;
  }
  // `sceneState` is captured weakly to avoid retaining a scene that
  // disconnects before the tab is opened.
  __weak SceneState* weakSceneState = sceneState;
  RemoteActorRequest pendingRequest = std::move(*request);
  ProceduralBlock completion = ^{
    HandleRemoteActorCredentialSharingRequest(
        weakSceneState.browserProviderInterface.currentBrowser, pendingRequest);
  };
  [self openTabWithSceneState:sceneState
                urlLoadParams:UrlLoadParams::InNewTab(GURL())
                   targetMode:ApplicationModeForTabOpening::UNDETERMINED
            postOpeningAction:TabOpeningPostOpeningAction::NO_ACTION
        postOpeningCompletion:completion];
}

@end
