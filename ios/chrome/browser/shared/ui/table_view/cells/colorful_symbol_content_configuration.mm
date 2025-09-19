// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/colorful_symbol_content_configuration.h"

#import "ios/chrome/browser/shared/ui/table_view/cells/colorful_symbol_content_view.h"

@implementation ColorfulSymbolContentConfiguration

#pragma mark - UIContentConfiguration

- (instancetype)copyWithZone:(NSZone*)zone {
  ColorfulSymbolContentConfiguration* copy =
      [[self.class allocWithZone:zone] init];
  // The updates to properties must be reflected in the copy method.
  // LINT.IfChange(Copy)
  copy.symbolImage = _symbolImage;
  copy.symbolBackgroundColor = _symbolBackgroundColor;
  copy.symbolTintColor = _symbolTintColor;
  // LINT.ThenChange(colorful_symbol_content_configuration.h:Copy)
  return copy;
}

- (id<UIContentConfiguration>)updatedConfigurationForState:
    (id<UIConfigurationState>)state {
  return self;
}

- (UIView*)makeContentView {
  return [[ColorfulSymbolContentView alloc] initWithConfiguration:self];
}

@end
