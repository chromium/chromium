// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/change_profile/change_profile_authentication_continuation.h"

#import "base/functional/callback_helpers.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_controller.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/qr_scanner_commands.h"

namespace {

// Implementation of the continuation that starts the sign-in or sign-out flow.
void ChangeProfileAuthenticationContinuation(WidgetContext* context,
                                             NSSet<UIOpenURLContext*>* contexts,
                                             SceneState* scene_state,
                                             base::OnceClosure closure) {
  // TODO(crbug.com/388520520): Move sign-in and sign-out logic out of
  // SceneController. The logic will be moved in the continuation.
  [scene_state.controller changeAccountForContext:context
                                     openContexts:contexts];
  std::move(closure).Run();
}

}  // namespace

ChangeProfileContinuation CreateChangeProfileAuthenticationContinuation(
    WidgetContext* context,
    NSSet<UIOpenURLContext*>* contexts) {
  return base::BindOnce(&ChangeProfileAuthenticationContinuation, context,
                        contexts);
}
