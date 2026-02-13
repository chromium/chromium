// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui/cells/standalone_module_view_configuration.h"

@implementation StandaloneModuleViewConfiguration

#pragma mark - NSCopying

- (instancetype)copyWithZone:(NSZone*)zone {
  StandaloneModuleViewConfiguration* viewConfig =
      [[super copyWithZone:zone] init];
  // The updates to properties must be reflected in the copy method.
  // LINT.IfChange(Copy)
  viewConfig.productImage = self.productImage;
  viewConfig.faviconImage = self.faviconImage;
  viewConfig.fallbackSymbolImage = self.fallbackSymbolImage;
  viewConfig.titleText = [self.titleText copy];
  viewConfig.bodyText = [self.bodyText copy];
  viewConfig.buttonText = [self.buttonText copy];
  viewConfig.accessibilityIdentifier = [self.accessibilityIdentifier copy];
  // LINT.ThenChange(standalone_module_view_configuration.h:Copy)
  return viewConfig;
}

@end
