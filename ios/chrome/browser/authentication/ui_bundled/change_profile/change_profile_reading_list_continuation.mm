// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/change_profile/change_profile_reading_list_continuation.h"

#import "base/functional/callback_helpers.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/public/commands/browser_coordinator_commands.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"

namespace {

// Implementation of the continuation opening the reading list view.
void ChangeProfileReadingListContinuation(SceneState* scene_state,
                                          base::OnceClosure closure) {
  Browser* browser =
      scene_state.browserProviderInterface.currentBrowserProvider.browser;
  CHECK(browser);

  CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
  id<BrowserCoordinatorCommands> browserCoordinatorHandler =
      HandlerForProtocol(dispatcher, BrowserCoordinatorCommands);
  [browserCoordinatorHandler showReadingList];

  std::move(closure).Run();
}

}  // namespace

ChangeProfileContinuation CreateChangeProfileReadingListContinuation() {
  return base::BindOnce(&ChangeProfileReadingListContinuation);
}
