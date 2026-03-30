// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/label_content_configuration.h"

#import "ios/chrome/browser/settings/autofill/autofill_ai/ui/label_content_view.h"

@implementation LabelContentConfiguration

- (instancetype)init {
  self = [super init];
  if (self) {
  }
  return self;
}

#pragma mark - ChromeContentConfiguration

- (UIView<ChromeContentView>*)makeChromeContentView {
  return [[LabelContentView alloc] initWithConfiguration:self];
}

- (CGSize)contentSize {
  UILabel* label = [[UILabel alloc] init];
  label.text = self.text;
  label.font = [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
  label.adjustsFontForContentSizeCategory = YES;
  return [label intrinsicContentSize];
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
  LabelContentConfiguration* configuration = [[self class] allocWithZone:zone];
  // LINT.IfChange(Copy)
  configuration.text = self.text;
  // LINT.ThenChange(label_content_configuration.h:Copy)
  return configuration;
}

@end
