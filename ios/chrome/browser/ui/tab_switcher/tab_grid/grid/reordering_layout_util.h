// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_REORDERING_LAYOUT_UTIL_H_
#define IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_REORDERING_LAYOUT_UTIL_H_

#import <UIKit/UIKit.h>

// Returns an array whose elements are copies with added transparency of the
// elements of the |base_attributes_array| array.
NSArray<__kindof UICollectionViewLayoutAttributes*>*
CopyAttributesArrayAndSetInactiveOpacity(NSArray* base_attributes_array);

// Returns a copy of the |base_attributes| object with added transparency.
UICollectionViewLayoutAttributes* CopyAttributesAndSetInactiveOpacity(
    UICollectionViewLayoutAttributes* base_attributes);

// Returns a copy of the |base_attributes| object with added opacity and
// scaling.
UICollectionViewLayoutAttributes* CopyAttributesAndSetActiveProperties(
    UICollectionViewLayoutAttributes* base_attributes);

#endif  // IOS_CHROME_BROWSER_UI_TAB_SWITCHER_TAB_GRID_GRID_REORDERING_LAYOUT_UTIL_H_
