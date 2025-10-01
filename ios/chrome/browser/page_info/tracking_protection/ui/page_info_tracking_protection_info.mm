// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/page_info/tracking_protection/ui/page_info_tracking_protection_info.h"

@implementation PageInfoTrackingProtectionInfo

- (instancetype)initWithHasTrackingProtectionException:
                    (BOOL)hasTrackingProtectionException
                        shouldShowTrackingProtectionUI:
                            (BOOL)shouldShowTrackingProtectionUI {
  self = [super init];
  if (self) {
    _hasTrackingProtectionException = hasTrackingProtectionException;
    _shouldShowTrackingProtectionUI = shouldShowTrackingProtectionUI;
  }
  return self;
}

@end
