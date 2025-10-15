// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_FILTER_CELL_H_
#define IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_FILTER_CELL_H_

#import <UIKit/UIKit.h>

#import "ios/chrome/browser/download/model/download_filter_util.h"

// Collection view cell representing an individual download filter option.
// Displays an icon and text label for each filter type (e.g., All, Documents,
// Images).
@interface DownloadFilterCell : UICollectionViewCell

// Configures the cell with the specified filter type, updating the display text
// and icon.
- (void)configureWithFilterType:(DownloadFilterType)filterType;

// Returns the calculated width for the download filter cell with the given
// filter type.
+ (CGFloat)cellSizeForFilterType:(DownloadFilterType)filterType;

// Returns the standard height for download filter cells.
+ (CGFloat)cellHeight;

@end

#endif  // IOS_CHROME_BROWSER_DOWNLOAD_UI_DOWNLOAD_LIST_DOWNLOAD_FILTER_CELL_H_
