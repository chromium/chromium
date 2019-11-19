// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/url_loading/test_app_url_loading_service.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

TestAppUrlLoadingService::TestAppUrlLoadingService() {}

void TestAppUrlLoadingService::LoadUrlInNewTab(const UrlLoadParams& params) {
  last_params = params;
  load_new_tab_call_count++;
}

ios::ChromeBrowserState* TestAppUrlLoadingService::GetCurrentBrowserState() {
  return currentBrowserState;
}
