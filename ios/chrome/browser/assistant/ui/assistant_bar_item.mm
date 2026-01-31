// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/assistant/ui/assistant_bar_item.h"

@implementation AssistantBarItem

- (instancetype)initWithImage:(UIImage*)image
           accessibilityLabel:(NSString*)accessibilityLabel
                       action:(void (^)(void))action {
  self = [super init];
  if (self) {
    _image = image;
    _accessibilityLabel = [accessibilityLabel copy];
    _action = [action copy];
  }
  return self;
}

@end
