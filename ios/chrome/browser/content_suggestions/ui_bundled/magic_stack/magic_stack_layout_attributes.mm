// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui_bundled/magic_stack/magic_stack_layout_attributes.h"

#import "base/apple/foundation_util.h"

@implementation MagicStackLayoutAttributes

- (BOOL)isEqual:(id)object {
  MagicStackLayoutAttributes* otherAttributes =
      base::apple::ObjCCast<MagicStackLayoutAttributes>(object);
  if (!otherAttributes) {
    return false;
  }

  return self.subviewAlpha == otherAttributes.subviewAlpha &&
         [super isEqual:object];
}

- (id)copyWithZone:(NSZone*)zone {
  id copy = [super copyWithZone:zone];

  MagicStackLayoutAttributes* typedCopy =
      base::apple::ObjCCast<MagicStackLayoutAttributes>(copy);

  if (!typedCopy) {
    return copy;
  }

  typedCopy.subviewAlpha = self.subviewAlpha;

  return typedCopy;
}

@end
