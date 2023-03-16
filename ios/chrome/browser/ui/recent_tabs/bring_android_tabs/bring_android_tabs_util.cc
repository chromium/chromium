// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/recent_tabs/bring_android_tabs/bring_android_tabs_util.h"
#include "components/prefs/pref_service.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state.h"

std::vector<std::unique_ptr<synced_sessions::DistantTab>>
PromptTabsForAndroidSwitcher(ChromeBrowserState* browser_state) {
  // TODO(crbug.com/1418114): Add implementation.
  return std::vector<std::unique_ptr<synced_sessions::DistantTab>>();
}

void OnBringAndroidTabsPromptDisplayed(PrefService* user_prefs) {
  // TODO(crbug.com/1418114): Add implementation.
}

void OnUserInteractWithBringAndroidTabsPrompt() {
  // TODO(crbug.com/1418114): Add implementation.
}
