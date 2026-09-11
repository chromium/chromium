// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/omnibox/ui/popup/omnibox_popup_util.h"

#import "ios/chrome/common/ui/util/ui_util.h"

BOOL ShouldApplyOmniboxPopoutLayout(UITraitCollection* traitCollection) {
  return IsRegularXRegularSizeClass(traitCollection);
}

BOOL ShouldApplyOmniboxPopoutLayout(id<UITraitEnvironment> environment) {
  return ShouldApplyOmniboxPopoutLayout(environment.traitCollection);
}
