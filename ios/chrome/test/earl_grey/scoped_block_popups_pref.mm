// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/scoped_block_popups_pref.h"

#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"

ScopedBlockPopupsPref::ScopedBlockPopupsPref(ContentSetting setting)
    : original_setting_([ChromeEarlGrey popupPrefValue]) {
  [ChromeEarlGrey setPopupPrefValue:setting];
}

ScopedBlockPopupsPref::~ScopedBlockPopupsPref() {
  [ChromeEarlGrey setPopupPrefValue:original_setting_];
}
