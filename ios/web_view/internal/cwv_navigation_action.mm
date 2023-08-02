// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web_view/public/cwv_navigation_action.h"

@implementation CWVNavigationAction
@synthesize request = _request;
@synthesize userInitiated = _userInitiated;
@synthesize navigationType = _navigationType;

- (instancetype)initWithRequest:(NSURLRequest*)request
                  userInitiated:(BOOL)userInitiated
                 navigationType:(CWVNavigationType)navigationType {
  self = [super init];
  if (self) {
    _userInitiated = userInitiated;
    _request = [request copy];
    _navigationType = navigationType;
  }
  return self;
}

@end
