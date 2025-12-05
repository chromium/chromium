// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/activity_indicator_content_configuration.h"

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/activity_indicator_content_view.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

namespace {
constexpr CGFloat kViewSize = 30;
}  // namespace

@implementation ActivityIndicatorContentConfiguration

- (instancetype)init {
  self = [super init];
  if (self) {
    _style = UIActivityIndicatorViewStyleMedium;
    _animating = YES;
  }
  return self;
}

#pragma mark - ChromeContentConfiguration

- (UIView<ChromeContentView>*)makeChromeContentView {
  return [[ActivityIndicatorCellContentView alloc] initWithConfiguration:self];
}

- (CGSize)contentSize {
  return CGSizeMake(kViewSize, kViewSize);
}

#pragma mark - UIContentConfiguration

- (UIView<UIContentView>*)makeContentView {
  return [self makeChromeContentView];
}

- (instancetype)updatedConfigurationForState:(id<UIConfigurationState>)state {
  return self;
}

#pragma mark - NSCopying

- (id)copyWithZone:(NSZone*)zone {
  ActivityIndicatorContentConfiguration* copy =
      [[ActivityIndicatorContentConfiguration allocWithZone:zone] init];
  // The updates to properties must be reflected in the copy method.
  // LINT.IfChange(Copy)
  copy.style = self.style;
  copy.color = self.color;
  copy.animating = self.animating;
  // LINT.ThenChange(activity_indicator_content_configuration.h:Copy)
  return copy;
}

@end
