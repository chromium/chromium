// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/internal/cwv_navigation_response_internal.h"

@implementation CWVNavigationResponse
@synthesize response = _response;
@synthesize forMainFrame = _forMainFrame;

- (instancetype)initWithResponse:(NSURLResponse*)response
                    forMainFrame:(BOOL)forMainFrame {
  self = [super init];
  if (self) {
    _response = response;
    _forMainFrame = forMainFrame;
  }
  return self;
}

@end
