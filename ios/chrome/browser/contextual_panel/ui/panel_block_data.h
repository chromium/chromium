// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_UI_PANEL_BLOCK_DATA_H_
#define IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_UI_PANEL_BLOCK_DATA_H_

#import <UIKit/UIKit.h>

@class PanelBlockData;

// Class representing the data necessary to display an info block in the UI.
@interface PanelBlockData : NSObject

// The type of the block
@property(nonatomic, readonly) NSString* blockType;

// A cell registration that can be used to dequeue a reeusable
// PanelItemCollectionViewCell for this info block.
// NOTE: The returned cell must be a `PanelItemCollectionViewCell` subclass.
@property(nonatomic, strong, readonly)
    UICollectionViewCellRegistration* cellRegistration;

- (instancetype)initWithBlockType:(NSString*)blockType
                 cellRegistration:
                     (UICollectionViewCellRegistration*)cellRegistration
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // IOS_CHROME_BROWSER_CONTEXTUAL_PANEL_UI_PANEL_BLOCK_DATA_H_
