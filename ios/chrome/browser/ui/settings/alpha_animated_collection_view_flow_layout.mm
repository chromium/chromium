// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/alpha_animated_collection_view_flow_layout.h"

#import <UIKit/UIKit.h>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation AlphaAnimatedCollectionViewFlowLayout

- (UICollectionViewLayoutAttributes*)
initialLayoutAttributesForAppearingItemAtIndexPath:(NSIndexPath*)itemIndexPath {
  UICollectionViewLayoutAttributes* attr = [[super
      initialLayoutAttributesForAppearingItemAtIndexPath:itemIndexPath] copy];
  attr.alpha = 0;
  return attr;
}

- (UICollectionViewLayoutAttributes*)
initialLayoutAttributesForAppearingSupplementaryElementOfKind:
    (NSString*)elementKind
                                                  atIndexPath:
                                                      (NSIndexPath*)
                                                          elementIndexPath {
  UICollectionViewLayoutAttributes* attr = [[super
      initialLayoutAttributesForAppearingSupplementaryElementOfKind:elementKind
                                                        atIndexPath:
                                                            elementIndexPath]
      copy];
  attr.alpha = 0;
  return attr;
}

- (UICollectionViewLayoutAttributes*)
finalLayoutAttributesForDisappearingItemAtIndexPath:
    (NSIndexPath*)itemIndexPath {
  UICollectionViewLayoutAttributes* attr = [[super
      finalLayoutAttributesForDisappearingItemAtIndexPath:itemIndexPath] copy];
  attr.alpha = 0;
  return attr;
}

- (UICollectionViewLayoutAttributes*)
finalLayoutAttributesForDisappearingSupplementaryElementOfKind:
    (NSString*)elementKind
                                                   atIndexPath:
                                                       (NSIndexPath*)
                                                           elementIndexPath {
  UICollectionViewLayoutAttributes* attr = [[super
      finalLayoutAttributesForDisappearingSupplementaryElementOfKind:elementKind
                                                         atIndexPath:
                                                             elementIndexPath]
      copy];
  attr.alpha = 0;
  return attr;
}
@end
