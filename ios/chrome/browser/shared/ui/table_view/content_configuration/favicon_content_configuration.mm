// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/favicon_content_configuration.h"

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/favicon_content_view.h"
#import "ios/chrome/common/ui/favicon/favicon_attributes.h"

@implementation FaviconContentConfiguration

#pragma mark - ChromeContentConfiguration

- (UIView<ChromeContentView>*)makeChromeContentView {
  return [[FaviconContentView alloc] initWithConfiguration:self];
}

#pragma mark - UIContentConfiguration

- (UIView*)makeContentView {
  return [self makeChromeContentView];
}

- (id<UIContentConfiguration>)updatedConfigurationForState:
    (id<UIConfigurationState>)state {
  return self;
}

#pragma mark - NSCopying

- (id)copyWithZone:(NSZone*)zone {
  FaviconContentConfiguration* copy =
      [[FaviconContentConfiguration alloc] init];

  // LINT.IfChange(Copy)
  copy.faviconAttributes = self.faviconAttributes;
  copy.badgeImage = self.badgeImage;
  copy.badgeAccessibilityID = self.badgeAccessibilityID;
  // LINT.ThenChange(favicon_content_configuration.h:Copy)

  return copy;
}

@end
