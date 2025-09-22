// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/image_content_configuration.h"

#import "ios/chrome/browser/shared/ui/table_view/cells/image_content_view.h"

@implementation ImageContentConfiguration

- (instancetype)init {
  self = [super init];
  if (self) {
    _imageContentMode = UIViewContentModeScaleAspectFit;
  }
  return self;
}

#pragma mark - UIContentConfiguration

- (UIView<UIContentView>*)makeContentView {
  return [[ImageContentView alloc] initWithConfiguration:self];
}

- (instancetype)updatedConfigurationForState:(id<UIConfigurationState>)state {
  // No state changes supported for now.
  return self;
}

#pragma mark - NSCopying

- (id)copyWithZone:(NSZone*)zone {
  ImageContentConfiguration* copy = [[ImageContentConfiguration alloc] init];
  // LINT.IfChange(Copy)
  copy.image = self.image;
  copy.imageSize = self.imageSize;
  copy.imageContentMode = self.imageContentMode;
  // LINT.ThenChange(image_content_configuration.h:Copy)
  return copy;
}

@end
