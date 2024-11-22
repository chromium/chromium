// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/model/test_scene_url_loading_service.h"

#import "ios/chrome/browser/url_loading/model/url_loading_browser_agent.h"

TestSceneUrlLoadingService::TestSceneUrlLoadingService() {}

void TestSceneUrlLoadingService::LoadUrlInNewTab(const UrlLoadParams& params) {
  last_params_ = params;
  load_new_tab_call_count_++;
}

Browser* TestSceneUrlLoadingService::GetCurrentBrowser() {
  return current_browser_;
}

UrlLoadingBrowserAgent* TestSceneUrlLoadingService::GetBrowserAgent(
    bool incognito) {
  if (incognito) {
    return UrlLoadingBrowserAgent::FromBrowser(otr_browser_);
  }
  return UrlLoadingBrowserAgent::FromBrowser(original_browser_);
}
