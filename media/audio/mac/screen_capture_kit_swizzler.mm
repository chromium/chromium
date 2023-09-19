// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "screen_capture_kit_swizzler.h"

#import <Foundation/NSObject.h>

@interface SCStreamManagerSwizzler : NSObject
@end

@implementation SCStreamManagerSwizzler

+ (id)requestUserPermissionForScreenCapture {
  // Returning nil indicates that the permission is granted.
  return nil;
}

@end

namespace media {

std::unique_ptr<base::apple::ScopedObjCClassSwizzler>
SwizzleScreenCaptureKit() {
  if (@available(macOS 13.0, *)) {
    // ScreenCaptureKit internally performs a TCC permission check before
    // attempting any operations. This requires access to the TCC daemon,
    // which we would like to avoid granting to helper processes due to
    // security concerns. Skipping this preliminary check is acceptable, as
    // permissions are afterward externally checked by system services. To
    // do this, we swizzle the private API +[SCStreamManager
    // requestUserPermissionForScreenCapture], always returning to the
    // caller that the process has permissions.
    return std::make_unique<base::apple::ScopedObjCClassSwizzler>(
        NSClassFromString(@"SCStreamManager"), [SCStreamManagerSwizzler class],
        @selector(requestUserPermissionForScreenCapture));
  }

  return nullptr;
}

}  // namespace media
