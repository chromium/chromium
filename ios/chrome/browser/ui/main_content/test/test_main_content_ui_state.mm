// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/main_content/test/test_main_content_ui_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TestMainContentUIState
@synthesize yContentOffset = _yContentOffset;
@synthesize scrolling = _scrolling;
@synthesize dragging = _dragging;
@end
