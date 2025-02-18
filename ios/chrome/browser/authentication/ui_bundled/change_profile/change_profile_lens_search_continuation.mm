// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/change_profile/change_profile_lens_search_continuation.h"

#import "base/functional/callback_helpers.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/lens_commands.h"
#import "ios/chrome/browser/shared/public/commands/open_lens_input_selection_command.h"

namespace {

// Implementation of the continuation that starts a lens search.
void ChangeProfileLensSearchContinuation(LensEntrypoint entry_point,
                                         SceneState* scene_state,
                                         base::OnceClosure closure) {
  Browser* browser =
      scene_state.browserProviderInterface.currentBrowserProvider.browser;
  CHECK(browser);

  id<LensCommands> lens_handler =
      HandlerForProtocol(browser->GetCommandDispatcher(), LensCommands);
  OpenLensInputSelectionCommand* command = [[OpenLensInputSelectionCommand
      alloc]
          initWithEntryPoint:entry_point
           presentationStyle:LensInputSelectionPresentationStyle::SlideFromRight
      presentationCompletion:nil];
  [lens_handler openLensInputSelection:command];

  std::move(closure).Run();
}

}  // namespace

ChangeProfileContinuation CreateChangeProfileLensSearchContinuation(
    LensEntrypoint entry_point) {
  return base::BindOnce(&ChangeProfileLensSearchContinuation, entry_point);
}
