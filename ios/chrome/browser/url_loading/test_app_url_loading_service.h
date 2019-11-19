// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_URL_LOADING_TEST_APP_URL_LOADING_SERVICE_H_
#define IOS_CHROME_BROWSER_URL_LOADING_TEST_APP_URL_LOADING_SERVICE_H_

#include "ios/chrome/browser/url_loading/app_url_loading_service.h"
#import "ios/chrome/browser/url_loading/url_loading_params.h"

namespace ios {
class ChromeBrowserState;
}

// Service used to manage url loading at application level.
class TestAppUrlLoadingService : public AppUrlLoadingService {
 public:
  TestAppUrlLoadingService();

  // Opens a url based on |command| in a new tab.
  void LoadUrlInNewTab(const UrlLoadParams& params) override;

  // Returns the current browser state.
  ios::ChromeBrowserState* GetCurrentBrowserState() override;

  // These are the last parameters passed to |LoadUrlInNewTab|.
  UrlLoadParams last_params;
  int load_new_tab_call_count = 0;

  // This can be set by the test.
  ios::ChromeBrowserState* currentBrowserState;
};

#endif  // IOS_CHROME_BROWSER_URL_LOADING_APP_URL_LOADING_SERVICE_H_
