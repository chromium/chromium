// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/change_profile/change_profile_focus_omnibox.h"

#import "base/functional/callback_helpers.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"

namespace {

// Implementation of the continuation focusing on the omnibox.
void ChangeProfileFocusOmniboxContinuation(SceneState* scene_state,
                                           base::OnceClosure closure) {
  Browser* browser =
      scene_state.browserProviderInterface.currentBrowserProvider.browser;
  CHECK(browser);

  id<OmniboxCommands> omniboxCommandsHandler =
      HandlerForProtocol(browser->GetCommandDispatcher(), OmniboxCommands);
  [omniboxCommandsHandler focusOmnibox];

  std::move(closure).Run();
}

}  // namespace

ChangeProfileContinuation CreateChangeProfileFocusOmniboxContinuation() {
  return base::BindOnce(&ChangeProfileFocusOmniboxContinuation);
}
