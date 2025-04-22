// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/change_profile/change_profile_open_ntp.h"

#import "base/functional/callback.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/shared/model/url/chrome_url_constants.h"
#import "ios/chrome/browser/shared/model/web_state_list/web_state_list.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "url/gurl.h"

namespace {

// Implementation of the continuation opening a New Tab page.
void ChangeProfileOpensNTPContinuation(SceneState* scene_state,
                                       base::OnceClosure closure) {
  Browser* browser =
      scene_state.browserProviderInterface.currentBrowserProvider.browser;
  CHECK(browser);

  web::WebState* web_state = browser->GetWebStateList()->GetActiveWebState();

  if (!IsVisibleURLNewTabPage(web_state)) {
    UrlLoadingBrowserAgent::FromBrowser(browser)->Load(
        UrlLoadParams::InNewTab(GURL(kChromeUINewTabURL)));
  }
  std::move(closure).Run();
}

}  // namespace

ChangeProfileContinuation CreateChangeProfileOpensNTPContinuation() {
  return base::BindOnce(&ChangeProfileOpensNTPContinuation);
}
