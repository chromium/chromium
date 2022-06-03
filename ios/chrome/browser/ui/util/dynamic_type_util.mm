// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/util/dynamic_type_util.h"

#include "base/metrics/histogram_macros.h"
#include "ios/chrome/common/ui/util/dynamic_type_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

UIFont* LocationBarSteadyViewFont(UIContentSizeCategory currentCategory) {
  return PreferredFontForTextStyleWithMaxCategory(
      UIFontTextStyleBody, currentCategory,
      UIContentSizeCategoryAccessibilityExtraLarge);
}
