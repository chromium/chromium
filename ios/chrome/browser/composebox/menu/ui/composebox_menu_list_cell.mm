// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_list_cell.h"

@implementation ComposeboxMenuListCell

- (UICollectionViewLayoutAttributes*)preferredLayoutAttributesFittingAttributes:
    (UICollectionViewLayoutAttributes*)layoutAttributes {
  UICollectionViewLayoutAttributes* attributes =
      [super preferredLayoutAttributesFittingAttributes:layoutAttributes];
  CGRect frame = attributes.frame;
  // Round up to nearest point to break fractional rounding loops that can
  // cause recursive layout crashes.
  frame.size.height = ceil(frame.size.height);
  attributes.frame = frame;
  return attributes;
}

@end
