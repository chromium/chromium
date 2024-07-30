// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_BANNER_TRANSLATE_TRANSLATE_INFOBAR_PLACEHOLDER_OVERLAY_COORDINATOR_H_
#define IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_BANNER_TRANSLATE_TRANSLATE_INFOBAR_PLACEHOLDER_OVERLAY_COORDINATOR_H_

#import "ios/chrome/browser/overlays/ui_bundled/infobar_banner/infobar_banner_overlay_coordinator.h"

// A coordinator that displays nothing, serving as a placeholder between before
// and after Translate banners so that no other overlays are presented while
// Translate is finishing. It will not present or manage any UI.
@interface TranslateInfobarPlaceholderOverlayCoordinator
    : OverlayRequestCoordinator
@end

#endif  // IOS_CHROME_BROWSER_OVERLAYS_UI_BUNDLED_INFOBAR_BANNER_TRANSLATE_TRANSLATE_INFOBAR_PLACEHOLDER_OVERLAY_COORDINATOR_H_
