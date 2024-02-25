// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/model/test_scene_url_loading_service.h"

TestSceneUrlLoadingService::TestSceneUrlLoadingService() {}

void TestSceneUrlLoadingService::LoadUrlInNewTab(const UrlLoadParams& params) {
  last_params_ = params;
  load_new_tab_call_count_++;
}

Browser* TestSceneUrlLoadingService::GetCurrentBrowser() {
  return current_browser_;
}
