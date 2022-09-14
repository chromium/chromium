// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_IMAGE_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_IMAGE_DATA_SOURCE_H_

#import <UIKit/UIKit.h>

// Protocol that the grid UI uses to asynchronously pull images for cells in the
// grid.
@protocol GridImageDataSource
// Requests the receiver to provide a snapshot image corresponding to
// `identifier`. `completion` is called with the image if it exists.
- (void)snapshotForIdentifier:(NSString*)identifier
                   completion:(void (^)(UIImage*))completion;
// Requests the receiver to provide a favicon image corresponding to
// `identifier`. `completion` is called with the image if it exists.
- (void)faviconForIdentifier:(NSString*)identifier
                  completion:(void (^)(UIImage*))completion;

// Asks the receiver to pre-fetch all of the snapshots for tabs that are likely
// to be visible, given that `gridSize` tabs can be displayed at once.
- (void)preloadSnapshotsForVisibleGridSize:(int)gridSize;

// Tells the receiver to dispose of any pre-loaded snapshots it may have cached.
- (void)clearPreloadedSnapshots;

@end

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_GRID_IMAGE_DATA_SOURCE_H_
