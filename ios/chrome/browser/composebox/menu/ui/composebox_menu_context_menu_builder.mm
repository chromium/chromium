// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/composebox/menu/ui/composebox_menu_context_menu_builder.h"

@implementation ComposeboxMenuContextMenuBuilder {
  // The input state this builder is based on.
  ComposeboxUIInputState* _inputState;
}

#pragma mark - Public

- (instancetype)initWithInputState:(ComposeboxUIInputState*)inputState {
  self = [super init];
  if (self) {
    _inputState = inputState;
  }

  return self;
}

- (UIMenu*)createMenu {
  // TODO(crbug.com/565747588): Implement.
  return [UIMenu menuWithTitle:@"" children:@[]];
}

@end
