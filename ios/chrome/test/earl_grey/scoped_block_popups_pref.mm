// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/test/earl_grey/scoped_block_popups_pref.h"

#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

ScopedBlockPopupsPref::ScopedBlockPopupsPref(ContentSetting setting)
    : original_setting_([ChromeEarlGrey popupPrefValue]) {
  [ChromeEarlGrey setPopupPrefValue:setting];
}

ScopedBlockPopupsPref::~ScopedBlockPopupsPref() {
  [ChromeEarlGrey setPopupPrefValue:original_setting_];
}
