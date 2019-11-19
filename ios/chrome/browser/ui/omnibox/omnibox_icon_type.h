// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_ICON_TYPE_H_
#define IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_ICON_TYPE_H_

#import <Foundation/Foundation.h>

// All available icons for security states.
enum LocationBarSecurityIconType {
  INFO = 0,
  SECURE,
  NOT_SECURE_WARNING,
  LOCATION_BAR_SECURITY_ICON_TYPE_COUNT,
};

// Returns the asset name (to be used in -[UIImage imageNamed:]).
NSString* GetLocationBarSecurityIconTypeAssetName(
    LocationBarSecurityIconType icon);

#endif  // IOS_CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_ICON_TYPE_H_
