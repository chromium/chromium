// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_TEST_EARL_GREY_SCOPED_BLOCK_POPUPS_PREF_H_
#define IOS_CHROME_TEST_EARL_GREY_SCOPED_BLOCK_POPUPS_PREF_H_

#include "components/content_settings/core/common/content_settings.h"

// ScopedBlockPopupsPref modifies the block popups preference for the original
// profile and resets the preference to its original value when this
// object goes out of scope.
class ScopedBlockPopupsPref {
 public:
  explicit ScopedBlockPopupsPref(ContentSetting setting);

  ScopedBlockPopupsPref(const ScopedBlockPopupsPref&) = delete;
  ScopedBlockPopupsPref& operator=(const ScopedBlockPopupsPref&) = delete;

  ~ScopedBlockPopupsPref();

 private:
  // Saves the original pref setting so that it can be restored when the scoper
  // is destroyed.
  ContentSetting original_setting_;
};

#endif  // IOS_CHROME_TEST_EARL_GREY_SCOPED_BLOCK_POPUPS_PREF_H_
