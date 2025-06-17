// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <UIKit/UIKit.h>

#import "ios/public/provider/chrome/browser/mini_map/mini_map_api.h"

namespace ios {
namespace provider {

id<MiniMapController> CreateMiniMapController() {
  // Mini map is not supported in Chromium.
  return nil;
}

BOOL MiniMapCanHandleURL(NSURL* url) {
  return NO;
}

}  // namespace provider
}  // namespace ios
