// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/app_utils/app_utils_api.h"

namespace ios {
namespace provider {

void Initialize() {
  // Chromium does not have global state to initialize.
}

void AppendSwitchesFromExperimentalSettings(
    NSUserDefaults* experimental_settings,
    base::CommandLine* command_line) {
  // Chromium does not have experimental settings.
}

void AttachBrowserAgents(Browser* browser) {
  // Chromium does not attach additional browser agents.
}

}  // namespace provider
}  // namespace ios
