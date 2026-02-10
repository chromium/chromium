// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/content_suggestions/ui/cells/standalone_module_view_configuration.h"

@implementation StandaloneModuleViewConfiguration

#pragma mark - NSCopying

- (id)copyWithZone:(NSZone*)zone {
  StandaloneModuleViewConfiguration* copy = [[super copyWithZone:zone] init];
  copy.productImage = self.productImage;
  copy.faviconImage = self.faviconImage;
  copy.fallbackSymbolImage = self.fallbackSymbolImage;
  copy.titleText = self.titleText;
  copy.bodyText = self.bodyText;
  copy.buttonText = self.buttonText;
  copy.accessibilityIdentifier = copy.accessibilityIdentifier;
  return copy;
}

@end
