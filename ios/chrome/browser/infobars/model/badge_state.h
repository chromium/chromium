// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_INFOBARS_MODEL_BADGE_STATE_H_
#define IOS_CHROME_BROWSER_INFOBARS_MODEL_BADGE_STATE_H_

#import <Foundation/Foundation.h>

// States for the InfobarBadge.
typedef NS_OPTIONS(NSUInteger, BadgeState) {
  // The badge has not been accepted nor has it been read.
  BadgeStateNone = 0,
  // This property is set if it is read (i.e. the menu is opened, if it is set
  // as the displayed badge, or if the user has accepted the badge action).
  // Not set if the user has not seen the badge yet (e.g. the badge is in the
  // overflow menu and the user has yet to open the menu).
  BadgeStateRead = 1 << 0,
  // The badge's banner is currently being presented.
  BadgeStatePresented = 1 << 1,
  // The Infobar Badge is accepted. e.g. The Infobar was accepted/confirmed, and
  // the Infobar action has taken place.
  BadgeStateAccepted = 1 << 2,
};

#endif  // IOS_CHROME_BROWSER_INFOBARS_MODEL_BADGE_STATE_H_
