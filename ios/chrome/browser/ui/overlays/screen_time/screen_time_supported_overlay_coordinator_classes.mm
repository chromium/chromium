// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/overlays/screen_time/screen_time_supported_overlay_coordinator_classes.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace screen_time {

NSArray<Class>* GetSupportedOverlayCoordinatorClasses() {
  // TODO(crbug.com/1123696): Create ScreenTime coordinator.
  return @[];
}

}  // screen_time
