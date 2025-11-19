// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/image_content_configuration.h"

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/image_content_view.h"

@implementation ImageContentConfiguration

- (instancetype)init {
  self = [super init];
  if (self) {
    _imageContentMode = UIViewContentModeScaleAspectFit;
    _imageSize = CGSizeZero;
  }
  return self;
}

#pragma mark - ChromeContentConfiguration

- (UIView<ChromeContentView>*)makeChromeContentView {
  return [[ImageContentView alloc] initWithConfiguration:self];
}

#pragma mark - UIContentConfiguration

- (id<UIContentView>)makeContentView {
  return [self makeChromeContentView];
}

- (instancetype)updatedConfigurationForState:(id<UIConfigurationState>)state {
  return self;
}

#pragma mark - NSCopying

- (id)copyWithZone:(NSZone*)zone {
  ImageContentConfiguration* copy = [[ImageContentConfiguration alloc] init];
  // LINT.IfChange(Copy)
  copy.image = self.image;
  copy.imageSize = self.imageSize;
  copy.imageContentMode = self.imageContentMode;
  copy.imageTintColor = self.imageTintColor;
  copy.accessibilityID = self.accessibilityID;
  // LINT.ThenChange(image_content_configuration.h:Copy)
  return copy;
}

@end
