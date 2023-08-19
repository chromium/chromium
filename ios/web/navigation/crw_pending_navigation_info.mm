// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/navigation/crw_pending_navigation_info.h"

@implementation CRWPendingNavigationInfo

- (instancetype)init {
  if ((self = [super init])) {
    _navigationType = WKNavigationTypeOther;
  }
  return self;
}

@end
