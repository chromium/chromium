// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/ui/panel_block_data.h"

#import "base/check.h"
#import "ios/chrome/browser/contextual_panel/ui/panel_item_collection_view_cell.h"

@implementation PanelBlockData

- (instancetype)initWithBlockType:(NSString*)blockType
                 cellRegistration:
                     (UICollectionViewCellRegistration*)cellRegistration {
  if ((self = [super init])) {
    _blockType = blockType;
    DCHECK([cellRegistration.cellClass
        isSubclassOfClass:[PanelItemCollectionViewCell class]]);
    _cellRegistration = cellRegistration;
  }
  return self;
}

@end
