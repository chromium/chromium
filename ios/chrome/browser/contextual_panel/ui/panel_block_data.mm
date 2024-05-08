// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/ui/panel_block_data.h"

@implementation PanelBlockData

- (instancetype)initWithBlockType:(NSString*)blockType
                 cellRegistration:
                     (UICollectionViewCellRegistration*)cellRegistration {
  if (self = [super init]) {
    _blockType = blockType;
    _cellRegistration = cellRegistration;
  }
  return self;
}

@end
