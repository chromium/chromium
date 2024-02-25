// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_URL_LOADING_MODEL_TEST_SCENE_URL_LOADING_SERVICE_H_
#define IOS_CHROME_BROWSER_URL_LOADING_MODEL_TEST_SCENE_URL_LOADING_SERVICE_H_

#import "base/memory/raw_ptr.h"
#include "ios/chrome/browser/url_loading/model/scene_url_loading_service.h"
#import "ios/chrome/browser/url_loading/model/url_loading_params.h"

class Browser;

// Service used to manage url loading at application level.
class TestSceneUrlLoadingService : public SceneUrlLoadingService {
 public:
  TestSceneUrlLoadingService();
  ~TestSceneUrlLoadingService() override {}

  // Stores `params` in the instance variables.
  void LoadUrlInNewTab(const UrlLoadParams& params) override;

  // Returns the current browser.
  Browser* GetCurrentBrowser() override;

  // These are the last parameters passed to `LoadUrlInNewTab`.
  UrlLoadParams last_params_;
  int load_new_tab_call_count_ = 0;

  // This can be set by the test.
  raw_ptr<Browser> current_browser_;
};

#endif  // IOS_CHROME_BROWSER_URL_LOADING_MODEL_TEST_SCENE_URL_LOADING_SERVICE_H_
