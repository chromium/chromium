// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/util/vertical_bar_util.h"

#import "ios/chrome/browser/shared/public/features/features.h"

#if defined(__IPHONE_27_1) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_27_1

namespace {

// Whether the vertical bar should be disabled for `traits`. Only iPhone Duo
// opts out; other devices keep the system default. Only regular widths opt
// out, so the transient unspecified size class during view loading doesn't
// toggle the behavior (UIKit asks for it to be a stable choice).
// TODO(crbug.com/585252907): Replace `IsDuoDevice()` with the UIKit hinge API
// once the iOS 27.3 SDK is used.
bool ShouldDisableVerticalBar(UITraitCollection* traits) {
  return IsDuoDevice() &&
         traits.horizontalSizeClass == UIUserInterfaceSizeClassRegular;
}

}  // namespace

UIVerticalBarBehavior PreferredVerticalBarBehavior(UITraitCollection* traits) {
  return ShouldDisableVerticalBar(traits) ? UIVerticalBarBehaviorDisabled
                                          : UIVerticalBarBehaviorAutomatic;
}

#endif

bool IsVerticalBarActive(UITraitCollection* traits) {
#if defined(__IPHONE_27_1) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_27_1
  if (@available(iOS 27.1, *)) {
    // `verticalBarEdge` only updates once UIKit re-queries the root after a
    // trait change, so also check the root's current preference.
    return traits.verticalBarEdge != UIVerticalBarEdgeUnspecified &&
           !ShouldDisableVerticalBar(traits);
  }
#endif
  return false;
}

void UpdateVerticalBarConfiguration(UIViewController* view_controller) {
#if defined(__IPHONE_27_1) && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_27_1
  if (@available(iOS 27.1, *)) {
    [view_controller setNeedsUpdateOfVerticalBarConfiguration];
  }
#endif
}
