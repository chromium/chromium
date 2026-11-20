// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/browser/browser_utils.h"

#import "base/check_op.h"
#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_provider_interface.h"

Browser* GetRegularBrowserForScene(SceneState* scene_state) {
  CHECK(scene_state);
  Browser* regular_browser = scene_state.browserProviderInterface.mainBrowser;
  CHECK(regular_browser);
  CHECK_EQ(regular_browser->type(), Browser::Type::kRegular);
  return regular_browser;
}
