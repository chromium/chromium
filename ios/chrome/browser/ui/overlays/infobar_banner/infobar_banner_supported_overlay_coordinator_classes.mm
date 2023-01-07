// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_supported_overlay_coordinator_classes.h"

#import "ios/chrome/browser/ui/overlays/infobar_banner/infobar_banner_overlay_coordinator.h"
#import "ios/chrome/browser/ui/overlays/infobar_banner/translate/translate_infobar_placeholder_overlay_coordinator.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace infobar_banner {

NSArray<Class>* GetSupportedOverlayCoordinatorClasses() {
  return @[
    [InfobarBannerOverlayCoordinator class],
    [TranslateInfobarPlaceholderOverlayCoordinator class]
  ];
}

}  // infobar_banner
