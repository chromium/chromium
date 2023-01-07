// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_view_handler.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CRWWebViewHandler

- (void)close {
  _beingDestroyed = YES;
}

@end
