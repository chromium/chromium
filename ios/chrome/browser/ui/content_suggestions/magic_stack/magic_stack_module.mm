// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/content_suggestions/magic_stack/magic_stack_module.h"

@implementation MagicStackModule {
  // The hash of this identifier.
  NSUInteger _hash;
}

#pragma mark - NSObject

- (BOOL)isEqual:(id)object {
  if (self == object) {
    return YES;
  }
  if (![object isKindOfClass:[MagicStackModule class]]) {
    return NO;
  }
  MagicStackModule* moduleObject = static_cast<MagicStackModule*>(object);
  return self.type == moduleObject.type;
}

- (NSUInteger)hash {
  return @(int(self.type)).hash;
}

@end
