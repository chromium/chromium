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

void NotifyNewTabEvent(ChromeBrowserState* browserState, bool isIncognito) {
  const char* const event =
      isIncognito ? feature_engagement::events::kIncognitoTabOpened
                  : feature_engagement::events::kNewTabOpened;
  TrackerFactory::GetForBrowserState(browserState)
      ->NotifyEvent(std::string(event));
}

void NotifyNewTabEventForCommand(ChromeBrowserState* browserState,
                                 OpenNewTabCommand* command) {
  if (command.isUserInitiated) {
    NotifyNewTabEvent(browserState, command.inIncognito);
  }
}

}  // namespace feature_engagement
