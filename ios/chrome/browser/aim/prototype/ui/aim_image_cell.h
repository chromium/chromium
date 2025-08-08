// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_IMAGE_CELL_H_
#define IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_IMAGE_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/aim/prototype/ui/aim_input_item.h"

// Cell for displaying an image in the AIM prototype.
@interface AIMImageCell : UICollectionViewCell

- (void)configureWithItem:(AIMInputItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_AIM_PROTOTYPE_UI_AIM_IMAGE_CELL_H_
