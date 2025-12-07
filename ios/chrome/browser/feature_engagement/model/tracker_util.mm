// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/feature_engagement/model/tracker_util.h"

#import "components/feature_engagement/public/event_constants.h"
#import "components/feature_engagement/public/tracker.h"
#import "ios/chrome/browser/feature_engagement/model/tracker_factory.h"

void feature_engagement::NotifyNewTabEvent(Tracker* tracker,
                                           bool is_incognito) {
  const char* const event =
      is_incognito ? feature_engagement::events::kIncognitoTabOpened
                   : feature_engagement::events::kNewTabOpened;
  tracker->NotifyEvent(std::string(event));
}
