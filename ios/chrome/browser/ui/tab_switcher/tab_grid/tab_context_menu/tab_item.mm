// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_item.h"

#import "url/gurl.h"

@implementation TabItem

- (instancetype)initWithTitle:(NSString*)title URL:(GURL)URL {
  if ((self = [super init])) {
    _title = title;
    _URL = URL;
  }
  return self;
}

@end
