// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/dynamic_type_util.h"

#import "base/metrics/histogram_macros.h"
#import "ios/chrome/common/ui/util/dynamic_type_util.h"

UIFont* LocationBarSteadyViewFont(UIContentSizeCategory currentCategory) {
  return PreferredFontForTextStyleWithMaxCategory(
      UIFontTextStyleBody, currentCategory,
      UIContentSizeCategoryAccessibilityExtraLarge);
}
