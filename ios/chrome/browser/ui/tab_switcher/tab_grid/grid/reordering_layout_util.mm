// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/reordering_layout_util.h"

#import "ios/chrome/browser/ui/tab_switcher/tab_grid/grid/grid_constants.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

NSArray<__kindof UICollectionViewLayoutAttributes*>*
CopyAttributesArrayAndSetInactiveOpacity(NSArray* base_attributes_array) {
  NSMutableArray<__kindof UICollectionViewLayoutAttributes*>* attributes_array =
      [NSMutableArray array];
  for (UICollectionViewLayoutAttributes* attributes in base_attributes_array) {
    UICollectionViewLayoutAttributes* new_attributes =
        CopyAttributesAndSetInactiveOpacity(attributes);
    [attributes_array addObject:new_attributes];
  }
  return [attributes_array copy];
}

UICollectionViewLayoutAttributes* CopyAttributesAndSetInactiveOpacity(
    UICollectionViewLayoutAttributes* base_attributes) {
  UICollectionViewLayoutAttributes* attributes = [base_attributes copy];
  attributes.alpha = kReorderingInactiveCellOpacity;
  return attributes;
}

UICollectionViewLayoutAttributes* CopyAttributesAndSetActiveProperties(
    UICollectionViewLayoutAttributes* base_attributes) {
  UICollectionViewLayoutAttributes* attributes = [base_attributes copy];
  // The moving item has regular opacity, but is scaled.
  attributes.alpha = 1.0;
  attributes.transform =
      CGAffineTransformScale(attributes.transform, kReorderingActiveCellScale,
                             kReorderingActiveCellScale);
  return attributes;
}
