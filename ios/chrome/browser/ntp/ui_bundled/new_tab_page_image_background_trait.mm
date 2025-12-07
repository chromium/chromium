// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ntp/ui_bundled/new_tab_page_image_background_trait.h"

#import "base/apple/foundation_util.h"

@implementation NewTabPageImageBackgroundTrait

+ (BOOL)defaultBoolValue {
  return NO;
}

#pragma mark - UIObjectTraitDefinition

+ (NSNumber*)defaultValue {
  return [NSNumber numberWithBool:[self defaultBoolValue]];
}

@end

@implementation CustomUITraitAccessor (NewTabPageImageBackgroundTrait)

- (void)setBoolForNewTabPageImageBackgroundTrait:(BOOL)boolean {
  [self.mutableTraits setObject:[NSNumber numberWithBool:boolean]
                       forTrait:NewTabPageImageBackgroundTrait.class];
}

- (BOOL)boolForNewTabPageImageBackgroundTrait {
  return [base::apple::ObjCCastStrict<NSNumber>([self.mutableTraits
      objectForTrait:NewTabPageImageBackgroundTrait.class]) boolValue];
}

@end

@implementation UITraitCollection (NewTabPageImageBackgroundTrait)

- (BOOL)boolForNewTabPageImageBackgroundTrait {
  return [base::apple::ObjCCastStrict<NSNumber>(
      [self objectForTrait:NewTabPageImageBackgroundTrait.class]) boolValue];
}

@end
