// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/info_button_content_configuration.h"

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/info_button_content_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation InfoButtonContentConfiguration

- (instancetype)init {
  self = [super init];
  if (self) {
    _enabled = YES;
    _selectedForVoiceOver = YES;
  }
  return self;
}

#pragma mark - ChromeContentConfiguration

- (UIView<ChromeContentView>*)makeChromeContentView {
  return [[InfoButtonContentView alloc] initWithConfiguration:self];
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
  InfoButtonContentConfiguration* configuration =
      [[self class] allocWithZone:zone];
  // LINT.IfChange(Copy)
  configuration.target = self.target;
  configuration.selector = self.selector;
  configuration.enabled = self.enabled;
  configuration.tag = self.tag;
  configuration.selectedForVoiceOver = self.selectedForVoiceOver;
  // LINT.ThenChange(info_button_content_configuration.h:Copy)
  return configuration;
}

#pragma mark - Accessibility

- (NSString*)accessibilityHint {
  if (self.enabled) {
    return l10n_util::GetNSString(IDS_IOS_INFO_BUTTON_ACCESSIBILITY_HINT);
  }
  return nil;
}

@end
