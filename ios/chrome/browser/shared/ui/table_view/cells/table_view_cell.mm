// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/chrome_content_view.h"

@implementation TableViewCell

#pragma mark - UITableViewCell

- (void)prepareForReuse {
  [super prepareForReuse];
  self.accessoryType = UITableViewCellAccessoryNone;
  self.selectionStyle = UITableViewCellSelectionStyleDefault;
  self.userInteractionEnabled = YES;
  self.accessibilityLabel = nil;
  self.accessibilityHint = nil;
  self.accessibilityValue = nil;
  self.accessibilityUserInputLabels = nil;
  self.accessoryView = nil;
}

#pragma mark - Accessibility

- (NSString*)accessibilityLabel {
  NSObject* contentConfiguration = self.contentConfiguration;
  if (contentConfiguration.accessibilityLabel) {
    return contentConfiguration.accessibilityLabel;
  }
  return [super accessibilityLabel];
}

- (NSString*)accessibilityValue {
  NSObject* contentConfiguration = self.contentConfiguration;
  if (contentConfiguration.accessibilityValue) {
    return contentConfiguration.accessibilityValue;
  }
  return [super accessibilityValue];
}

- (NSString*)accessibilityHint {
  NSObject* contentConfiguration = self.contentConfiguration;
  if (contentConfiguration.accessibilityHint) {
    return contentConfiguration.accessibilityHint;
  }
  return [super accessibilityHint];
}

- (NSArray<NSString*>*)accessibilityUserInputLabels {
  NSObject* contentConfiguration = self.contentConfiguration;
  if (contentConfiguration.accessibilityUserInputLabels) {
    return contentConfiguration.accessibilityUserInputLabels;
  }
  return [super accessibilityUserInputLabels];
}

- (CGPoint)accessibilityActivationPoint {
  if ([self.contentView conformsToProtocol:@protocol(ChromeContentView)]) {
    UIView<ChromeContentView>* chromeContentView =
        static_cast<UIView<ChromeContentView>*>(self.contentView);
    if ([chromeContentView hasCustomAccessibilityActivationPoint]) {
      return chromeContentView.accessibilityActivationPoint;
    }
  }
  return [super accessibilityActivationPoint];
}

@end
