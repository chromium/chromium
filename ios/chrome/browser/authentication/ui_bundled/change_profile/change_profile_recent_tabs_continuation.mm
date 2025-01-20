// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/change_profile/change_profile_recent_tabs_continuation.h"

#import "base/functional/callback_helpers.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"

namespace {

// Implementation of the continuation opening the recent tabs view.
void ChangeProfileRecentTabsContinuation(SceneState* scene_state,
                                         base::OnceClosure closure) {
  Browser* browser =
      scene_state.browserProviderInterface.currentBrowserProvider.browser;
  CHECK(browser);

  id<BrowserCoordinatorCommands> browserCoordinatorHandler = HandlerForProtocol(
      browser->GetCommandDispatcher(), BrowserCoordinatorCommands);
  [browserCoordinatorHandler showRecentTabs];

  std::move(closure).Run();
}

}  // namespace

ChangeProfileContinuation CreateChangeProfileRecentTabsContinuation() {
  return base::BindOnce(&ChangeProfileRecentTabsContinuation);
}
