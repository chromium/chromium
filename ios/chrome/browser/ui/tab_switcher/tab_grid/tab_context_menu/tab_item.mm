// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_item.h"

#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation TabItem

- (instancetype)initWithTitle:(NSString*)title URL:(GURL)URL {
  if ((self = [super init])) {
    _title = title;
    _URL = URL;
  }
  return self;
}

@end
