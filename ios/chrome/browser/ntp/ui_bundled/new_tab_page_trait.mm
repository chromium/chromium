// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_trait.h"

#import "base/apple/foundation_util.h"
#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_color_palette.h"

@implementation NewTabPageTrait

#pragma mark - UIObjectTraitDefinition

+ (NewTabPageColorPalette*)defaultValue {
  return nil;
}

@end

@implementation CustomUITraitAccessor (NewTabPageTrait)

- (void)setObjectForNewTabPageTrait:(NewTabPageColorPalette*)object {
  [self.mutableTraits setObject:object forTrait:NewTabPageTrait.class];
}

- (NewTabPageColorPalette*)objectForNewTabPageTrait {
  return base::apple::ObjCCastStrict<NewTabPageColorPalette>(
      [self.mutableTraits objectForTrait:NewTabPageTrait.class]);
}

@end

@implementation UITraitCollection (NewTabPageTrait)

- (NewTabPageColorPalette*)objectForNewTabPageTrait {
  return base::apple::ObjCCastStrict<NewTabPageColorPalette>(
      [self objectForTrait:[NewTabPageTrait class]]);
}

@end
