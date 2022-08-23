// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/feature_engagement/tracker_util.h"

#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "ios/chrome/browser/feature_engagement/tracker_factory.h"
#import "ios/chrome/browser/ui/commands/open_new_tab_command.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace feature_engagement {

void NotifyNewTabEvent(ChromeBrowserState* browser_state, bool is_incognito) {
  const char* const event =
      is_incognito ? feature_engagement::events::kIncognitoTabOpened
                   : feature_engagement::events::kNewTabOpened;
  TrackerFactory::GetForBrowserState(browser_state)
      ->NotifyEvent(std::string(event));
}

void NotifyNewTabEventForCommand(ChromeBrowserState* browser_state,
                                 OpenNewTabCommand* command) {
  if (command.isUserInitiated) {
    NotifyNewTabEvent(browser_state, command.inIncognito);
  }
}

}  // namespace feature_engagement
