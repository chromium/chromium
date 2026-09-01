// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/change_profile/change_profile_send_tab.h"

#import "base/functional/callback_helpers.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/send_tab_to_self_commands.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "url/gurl.h"

namespace {

// Implementation of the continuation that opens the URL and show the UI to send
// tab to self.
void ChangeProfileSendTabToOtherDevice(
    GURL url,
    NSString* title,
    send_tab_to_self::ShareEntryPoint entry_point,
    SceneState* scene_state,
    base::OnceClosure closure) {
  Browser* browser =
      scene_state.browserProviderInterface.currentBrowserProvider.browser;
  CHECK(browser);

  UrlLoadingBrowserAgent::FromBrowser(browser)->Load(
      UrlLoadParams::InCurrentTab(url));
  CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
  id<SendTabToSelfCommands> sendTabToSelfHandler =
      HandlerForProtocol(dispatcher, SendTabToSelfCommands);

  [sendTabToSelfHandler showSendTabToSelfUI:url
                                      title:title
                                 entryPoint:entry_point];

  std::move(closure).Run();
}

}  // namespace

ChangeProfileContinuation CreateChangeProfileSendTabToOtherDevice(
    GURL url,
    NSString* title,
    send_tab_to_self::ShareEntryPoint entry_point) {
  return base::BindOnce(&ChangeProfileSendTabToOtherDevice, url, title,
                        entry_point);
}
