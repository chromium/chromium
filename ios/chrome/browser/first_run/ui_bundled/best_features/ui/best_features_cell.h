// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_UI_BEST_FEATURES_CELL_H_
#define IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_UI_BEST_FEATURES_CELL_H_

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"

@class BestFeaturesItem;

// Cell to layout a row in the table view for the Best Features Screen. Contains
// a leading icon image, title text, and detail text.
@interface BestFeaturesCell : TableViewCell

// Configures the cell for the given `item`.
- (void)setBestFeaturesItem:(BestFeaturesItem*)item;

@end

#endif  // IOS_CHROME_BROWSER_FIRST_RUN_UI_BUNDLED_BEST_FEATURES_UI_BEST_FEATURES_CELL_H_
