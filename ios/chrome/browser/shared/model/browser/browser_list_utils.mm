// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/model/browser/browser_list_utils.h"

#import "ios/chrome/browser/shared/coordinator/scene/scene_state.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"

namespace browser_list_utils {

Browser* GetMostActiveSceneBrowser(BrowserList* browser_list) {
  std::set<Browser*> all_browsers =
      browser_list->BrowsersOfType(BrowserList::BrowserType::kRegular);

  Browser* most_active_browser = nullptr;
  for (Browser* browser_to_check : all_browsers) {
    // The pointer to the scene state is weak, so it could be nil. In that case,
    // the activation level will be 0 (lowest).
    if (most_active_browser &&
        most_active_browser->GetSceneState().activationLevel >=
            browser_to_check->GetSceneState().activationLevel) {
      continue;
    }
    most_active_browser = browser_to_check;
    if (browser_to_check->GetSceneState().activationLevel ==
        SceneActivationLevelForegroundActive) {
      break;
    }
  }
  return most_active_browser;
}

}  // namespace browser_list_utils
