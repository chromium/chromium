// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/switch_content_configuration.h"

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/switch_content_view.h"

@implementation SwitchContentConfiguration

#pragma mark - UIContentConfiguration

- (id<UIContentView>)makeContentView {
  return [[SwitchContentView alloc] initWithConfiguration:self];
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
  configuration.tag = self.tag;
  // LINT.ThenChange(switch_content_configuration.h:Copy)
  return configuration;
}

@end
