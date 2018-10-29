// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/fullscreen/toolbar_ui.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ToolbarUIState
@synthesize collapsedHeight = _collapsedHeight;
@synthesize expandedHeight = _expandedHeight;
@synthesize bottomToolbarHeight = _bottomToolbarHeight;
@end
