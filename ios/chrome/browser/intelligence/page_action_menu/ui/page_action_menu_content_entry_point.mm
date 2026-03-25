// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/intelligence/page_action_menu/ui/page_action_menu_content_entry_point.h"

#import "base/check.h"

@implementation PageActionMenuContentEntryPoint

- (instancetype)initWithEnabled:(BOOL)enabled {
  return [self initWithEnabled:enabled footerItem:nil];
}

- (instancetype)initWithEnabled:(BOOL)enabled
                     footerItem:(ContentEntryPointUnavailabilityItem*)item {
  self = [super init];
  if (self) {
    _enabled = enabled;
    _unavailabilityItem = item;
  }
  return self;
}

@end

@implementation ContentEntryPointUnavailabilityItem

- (instancetype)initWithText:(NSString*)text {
  CHECK(text);
  return [self initWithText:text icon:nil actionIdentifier:nil];
}

- (instancetype)initWithText:(NSString*)text
                        icon:(UIImage*)icon
            actionIdentifier:(NSString*)actionIdentifier {
  CHECK(text);
  self = [super init];
  if (self) {
    _text = [text copy];
    _icon = icon;
    _actionIdentifier = [actionIdentifier copy];
  }
  return self;
}

@end
