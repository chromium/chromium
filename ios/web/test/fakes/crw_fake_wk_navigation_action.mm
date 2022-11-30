// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/fakes/crw_fake_wk_navigation_action.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CRWFakeWKNavigationAction
@synthesize sourceFrame = _sourceFrame;
@synthesize targetFrame = _targetFrame;
@synthesize navigationType = _navigationType;
@synthesize request = _request;
@end
