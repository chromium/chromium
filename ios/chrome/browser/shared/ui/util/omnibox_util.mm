// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/omnibox_util.h"

#import "ios/chrome/browser/omnibox/model/omnibox_position_browser_agent.h"
#import "ios/chrome/browser/shared/coordinator/layout_guide/layout_guide_util.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/ui/util/layout_guide_names.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/browser/shared/ui/util/util_swift.h"

bool IsCurrentLayoutBottomOmnibox(Browser* browser) {
  OmniboxPositionBrowserAgent* position_browser_agent =
      OmniboxPositionBrowserAgent::FromBrowser(browser);
  return position_browser_agent->IsCurrentLayoutBottomOmnibox();
}
