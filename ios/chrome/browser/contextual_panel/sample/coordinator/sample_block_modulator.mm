// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/contextual_panel/sample/coordinator/sample_block_modulator.h"

#import "ios/chrome/browser/contextual_panel/ui/panel_block_data.h"
#import "ios/chrome/browser/contextual_panel/ui/panel_item_collection_view_cell.h"

@implementation SampleBlockModulator {
  // The cell registration object this modulator uses for its ui.
  UICollectionViewCellRegistration* _cellRegistration;
}

- (void)start {
  _cellRegistration = [UICollectionViewCellRegistration
      registrationWithCellClass:[PanelItemCollectionViewCell class]
           configurationHandler:^(PanelItemCollectionViewCell* cell,
                                  NSIndexPath* indexPath, id item) {
             // Do any custom cell configuration here in the configuration
             // handler.
             cell.contentView.backgroundColor = UIColor.greenColor;
             [cell.contentView.heightAnchor constraintEqualToConstant:100]
                 .active = YES;
           }];
}

- (PanelBlockData*)panelBlockData {
  return [[PanelBlockData alloc] initWithBlockType:[self blockType]
                                  cellRegistration:_cellRegistration];
}

@end
