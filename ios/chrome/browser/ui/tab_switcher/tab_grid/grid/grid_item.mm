// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_item.h"

#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation GridItem

- (instancetype)initWithTitle:(NSString*)title url:(GURL)URL {
  if ((self = [super init])) {
    _title = title;
    _URL = URL;
  }
  return self;
}

@end
