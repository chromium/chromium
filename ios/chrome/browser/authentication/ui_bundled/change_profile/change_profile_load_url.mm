// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/authentication/ui_bundled/change_profile/change_profile_load_url.h"

#import "base/functional/callback_helpers.h"
#import "ios/chrome/browser/ntp/model/new_tab_page_util.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"
#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"
#import "ios/chrome/browser/window_activities/model/window_activity_helpers.h"
#import "url/gurl.h"

namespace {

// Implementation of the continuation opening a URL.
void ChangeProfileOpensURLContinuation(GURL url,
                                       SceneState* scene_state,
                                       base::OnceClosure closure) {
  Browser* browser =
      scene_state.browserProviderInterface.currentBrowserProvider.browser;
  CHECK(browser);

  web::WebState* web_state = browser->GetWebStateList()->GetActiveWebState();

  UrlLoadParams load_params = (IsVisibleURLNewTabPage(web_state))
                                  ? UrlLoadParams::InCurrentTab(url)
                                  : UrlLoadParams::InNewTab(url);
  UrlLoadingBrowserAgent::FromBrowser(browser)->Load(load_params);

  std::move(closure).Run();
}

}  // namespace

ChangeProfileContinuation CreateChangeProfileOpensURLContinuation(GURL url) {
  return base::BindOnce(&ChangeProfileOpensURLContinuation, url);
}
