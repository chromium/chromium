// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/fakes/crw_fake_wk_frame_info.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation CRWFakeWKFrameInfo
@synthesize mainFrame = _mainFrame;
@synthesize request = _request;
@synthesize securityOrigin = _securityOrigin;
@synthesize webView = _webView;
@end