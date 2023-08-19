// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/test/fakes/crw_fake_wk_frame_info.h"

@implementation CRWFakeWKFrameInfo
@synthesize mainFrame = _mainFrame;
@synthesize request = _request;
@synthesize securityOrigin = _securityOrigin;
@synthesize webView = _webView;
@end