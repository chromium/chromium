// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_CENTERED_FLOW_LAYOUT_H_
#define IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_CENTERED_FLOW_LAYOUT_H_

#import <UIKit/UIKit.h>

/**
 * A UICollectionViewFlowLayout subclass that centers items horizontally per
 * row.
 *
 * CenteredFlowLayout adjusts item positions so that each row of cells is
 * horizontally centered within the collection view's bounds, considering
 * sectionInsets. This centering is applied only if the total width of items
 * in a row is less than the available content width.
 * Item order within rows is maintained.
 */
@interface CenteredFlowLayout : UICollectionViewFlowLayout

@end

#endif  // IOS_CHROME_BROWSER_HOME_CUSTOMIZATION_UI_CENTERED_FLOW_LAYOUT_H_
