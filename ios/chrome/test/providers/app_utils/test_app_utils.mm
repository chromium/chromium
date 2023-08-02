// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/public/provider/chrome/browser/app_utils/app_utils_api.h"

namespace ios {
namespace provider {

void Initialize() {
  // Tests do not have global state to initialize.
}

void AppendSwitchesFromExperimentalSettings(
    NSUserDefaults* experimental_settings,
    base::CommandLine* command_line) {
  // Tests do not have experimental settings.
}

void AttachBrowserAgents(Browser* browser) {
  // Tests do not attach additional browser agents.
}

}  // namespace provider
}  // namespace ios
