// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/date_picker_content_configuration.h"

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/date_picker_content_view.h"

@implementation DatePickerContentConfiguration

- (instancetype)init {
  self = [super init];
  if (self) {
  }
  return self;
}

#pragma mark - ChromeContentConfiguration

- (UIView<ChromeContentView>*)makeChromeContentView {
  return [[DatePickerContentView alloc] initWithConfiguration:self];
}

- (CGSize)contentSize {
  static CGSize _cachedSize;
  static dispatch_once_t onceToken;

  dispatch_once(&onceToken, ^{
    UIView* view = [self makeContentView];
    _cachedSize =
        [view systemLayoutSizeFittingSize:UILayoutFittingCompressedSize];
  });

  return _cachedSize;
}

#pragma mark - UIContentConfiguration

- (id<UIContentView>)makeContentView {
  return [self makeChromeContentView];
}

- (instancetype)updatedConfigurationForState:(id<UIConfigurationState>)state {
  return self;
}

#pragma mark - NSCopying

- (instancetype)copyWithZone:(NSZone*)zone {
  DatePickerContentConfiguration* configuration =
      [[self class] allocWithZone:zone];
  // LINT.IfChange(Copy)
  configuration.target = self.target;
  configuration.selector = self.selector;
  configuration.userInteractionEnabled = self.userInteractionEnabled;
  configuration.date = self.date;
  // LINT.ThenChange(date_picker_content_configuration.h:Copy)
  return configuration;
}

@end
