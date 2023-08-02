// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/tab_context_menu/tab_cell.h"

@implementation TabCell

- (BOOL)hasIdentifier:(NSString*)identifier {
  return [self.itemIdentifier isEqualToString:identifier];
}

@end
