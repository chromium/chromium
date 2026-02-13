// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/magic_stack/ui/magic_stack_module.h"

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

- (BOOL)hasDifferentContentsFromConfig:(MagicStackModule*)config {
  return self.type != config.type;
}

#pragma mark - NSCopying

- (id)copyWithZone:(NSZone*)zone {
  MagicStackModule* copy = [[[self class] allocWithZone:zone] init];
  // The updates to properties must be reflected in the copy method.
  // LINT.IfChange(Copy)
  copy.shouldShowSeeMore = self.shouldShowSeeMore;
  copy.showNotificationsOptIn = self.showNotificationsOptIn;
  copy.delegate = self.delegate;
  // LINT.ThenChange(magic_stack_module.h:Copy)
  return copy;
}

@end
