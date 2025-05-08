// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/lens_overlay/model/lens_overlay_presentation_type.h"

#import "ios/chrome/common/ui/util/ui_util.h"
#import "ui/base/device_form_factor.h"

namespace lens {

ContainerPresentationType ContainerPresentationFor(
    id<UITraitEnvironment> environment) {
  return IsRegularXRegularSizeClass(environment)
             ? ContainerPresentationType::kContentAreaCover
             : ContainerPresentationType::kFullscreenCover;
}

ResultPagePresentationType ResultPagePresentationFor(
    id<UITraitEnvironment> environment) {
  return IsRegularXRegularSizeClass(environment)
             ? ResultPagePresentationType::kSidePanel
             : ResultPagePresentationType::kEdgeAttachedBottomSheet;
}

}  // namespace lens
