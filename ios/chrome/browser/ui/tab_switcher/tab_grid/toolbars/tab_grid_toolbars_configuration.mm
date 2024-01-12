// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/toolbars/tab_grid_toolbars_configuration.h"

@implementation TabGridToolbarsConfiguration

+ (TabGridToolbarsConfiguration*)disabledConfigurationForPage:
    (TabGridPage)page {
  return [[self alloc] initWithPage:page];
}

- (instancetype)initWithPage:(TabGridPage)page {
  self = [super init];
  if (self) {
    _page = page;
  }
  return self;
}

@end
