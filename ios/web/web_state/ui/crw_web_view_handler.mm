// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/web_state/ui/crw_web_view_handler.h"

@implementation CRWWebViewHandler

- (void)close {
  _beingDestroyed = YES;
}

@end
