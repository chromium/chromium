// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/bwg/utils/gemini_live_utils.h"

#import "ios/chrome/browser/intelligence/features/features.h"
#import "ios/chrome/browser/shared/model/browser/browser.h"
#import "ios/chrome/browser/shared/model/browser/browser_list.h"
#import "ios/chrome/browser/shared/model/browser/browser_list_factory.h"
#import "ios/chrome/browser/shared/model/profile/profile_ios.h"
#import "ios/chrome/browser/shared/public/commands/command_dispatcher.h"
#import "ios/chrome/browser/shared/public/commands/custom_leading_view_type.h"
#import "ios/chrome/browser/shared/public/commands/location_bar_badge_commands.h"
#import "ios/chrome/browser/shared/public/commands/omnibox_commands.h"
#import "ios/chrome/browser/shared/public/features/features.h"

namespace gemini {

void UpdateGeminiLiveIconVisibility(ProfileIOS* profile, bool in_live_mode) {
  if (!profile || !IsGeminiLiveEnabled()) {
    return;
  }

  BrowserList* browserList = BrowserListFactory::GetForProfile(profile);
  if (!browserList) {
    return;
  }

  CustomLeadingViewType type = in_live_mode ? CustomLeadingViewType::kGeminiLive
                                            : CustomLeadingViewType::kNone;

  for (Browser* browser :
       browserList->BrowsersOfType(BrowserList::BrowserType::kRegular)) {
    CommandDispatcher* dispatcher = browser->GetCommandDispatcher();
    if (IsChromeNextIaEnabled()) {
      id<LocationBarBadgeCommands> locationBarBadgeHandler =
          HandlerForProtocol(dispatcher, LocationBarBadgeCommands);
      [locationBarBadgeHandler setBadgeCustomLeadingViewType:type];
    } else {
      id<OmniboxCommands> omniboxHandler =
          HandlerForProtocol(dispatcher, OmniboxCommands);
      [omniboxHandler setCustomLeadingViewType:type];
    }
  }
}

}  // namespace gemini
