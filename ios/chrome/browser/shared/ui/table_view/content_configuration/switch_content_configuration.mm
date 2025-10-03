// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/switch_content_configuration.h"

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/switch_content_view.h"
#import "ios/chrome/grit/ios_strings.h"
#import "ui/base/l10n/l10n_util_mac.h"

@implementation SwitchContentConfiguration

- (instancetype)init {
  self = [super init];
  if (self) {
    _enabled = YES;
  }
  return self;
}

#pragma mark - ChromeContentConfiguration

- (UIView<ChromeContentView>*)makeChromeContentView {
  return [[SwitchContentView alloc] initWithConfiguration:self];
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
  SwitchContentConfiguration* configuration = [[self class] allocWithZone:zone];
  // LINT.IfChange(Copy)
  configuration.target = self.target;
  configuration.selector = self.selector;
  configuration.on = self.on;
  configuration.enabled = self.enabled;
  configuration.tag = self.tag;
  // LINT.ThenChange(switch_content_configuration.h:Copy)
  return configuration;
}

#pragma mark - Accessibility

- (NSString*)accessibilityHint {
  if (self.enabled) {
    return l10n_util::GetNSString(IDS_IOS_TOGGLE_SWITCH_ACCESSIBILITY_HINT);
  }
  return nil;
}

- (NSString*)accessibilityValue {
  if (self.on) {
    return l10n_util::GetNSString(IDS_IOS_SETTING_ON);
  } else {
    return l10n_util::GetNSString(IDS_IOS_SETTING_OFF);
  }
}

@end
