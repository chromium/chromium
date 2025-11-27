// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_configuration.h"

#import "ios/chrome/browser/shared/ui/table_view/cells/legacy_table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/chrome_content_configuration.h"
#import "ios/chrome/browser/shared/ui/table_view/content_configuration/table_view_cell_content_view.h"

@implementation TableViewCellContentConfiguration

- (instancetype)init {
  self = [super init];
  if (self) {
    _titleNumberOfLines = 0;
    _subtitleNumberOfLines = 0;
    _trailingTextNumberOfLines = 1;
    _titleLineBreakMode = NSLineBreakByWordWrapping;
    _subtitleLineBreakMode = NSLineBreakByWordWrapping;
  }
  return self;
}

#pragma mark - Public

+ (void)registerCellForTableView:(UITableView*)tableView {
  [tableView registerClass:TableViewCell.class
      forCellReuseIdentifier:NSStringFromClass(self.class)];
}

+ (TableViewCell*)dequeueTableViewCell:(UITableView*)tableView {
  TableViewCell* cell = [tableView
      dequeueReusableCellWithIdentifier:NSStringFromClass(self.class)];
  cell.isAccessibilityElement = YES;
  return cell;
}

+ (UITableViewCell*)dequeueTableViewCell:(UITableView*)tableView
                            forIndexPath:(NSIndexPath*)indexPath {
  TableViewCell* cell =
      [tableView dequeueReusableCellWithIdentifier:NSStringFromClass(self.class)
                                      forIndexPath:indexPath];
  cell.isAccessibilityElement = YES;
  return cell;
}

+ (void)legacyRegisterCellForTableView:(UITableView*)tableView {
  [tableView registerClass:LegacyTableViewCell.class
      forCellReuseIdentifier:NSStringFromClass(self.class)];
}

+ (LegacyTableViewCell*)legacyDequeueTableViewCell:(UITableView*)tableView {
  LegacyTableViewCell* cell = [tableView
      dequeueReusableCellWithIdentifier:NSStringFromClass(self.class)];
  cell.isAccessibilityElement = YES;
  return cell;
}

- (UIView*)makeAccessibilityConfiguredContentView {
  UIView* contentView = [self makeContentView];
  contentView.isAccessibilityElement = YES;
  contentView.accessibilityLabel = [self accessibilityLabel];
  contentView.accessibilityValue = [self accessibilityValue];
  contentView.accessibilityHint = [self accessibilityHint];
  contentView.accessibilityUserInputLabels =
      [self accessibilityUserInputLabels];
  return contentView;
}

#pragma mark - UIContentConfiguration

- (UIView<UIContentView>*)makeContentView {
  return [[TableViewCellContentView alloc] initWithConfiguration:self];
}

- (instancetype)updatedConfigurationForState:(id<UIConfigurationState>)state {
  return self;
}

#pragma mark - NSCopying

- (id)copyWithZone:(NSZone*)zone {
  TableViewCellContentConfiguration* copy =
      [[TableViewCellContentConfiguration allocWithZone:zone] init];
  // The updates to properties must be reflected in the copy method.
  // LINT.IfChange(Copy)
  copy.leadingConfiguration = [self.leadingConfiguration copyWithZone:zone];
  copy.trailingConfiguration = [self.trailingConfiguration copyWithZone:zone];
  copy.textDisabled = self.textDisabled;
  copy.title = self.title;
  copy.attributedTitle = self.attributedTitle;
  copy.titleColor = self.titleColor;
  copy.titleNumberOfLines = self.titleNumberOfLines;
  copy.titleLineBreakMode = self.titleLineBreakMode;
  copy.subtitle = self.subtitle;
  copy.attributedSubtitle = self.attributedSubtitle;
  copy.subtitleColor = self.subtitleColor;
  copy.subtitleNumberOfLines = self.subtitleNumberOfLines;
  copy.subtitleLineBreakMode = self.subtitleLineBreakMode;
  copy.secondSubtitle = self.secondSubtitle;
  copy.secondSubtitleNumberOfLines = self.secondSubtitleNumberOfLines;
  copy.trailingText = self.trailingText;
  copy.attributedTrailingText = self.attributedTrailingText;
  copy.trailingTextColor = self.trailingTextColor;
  copy.trailingTextNumberOfLines = self.trailingTextNumberOfLines;
  copy.customAccessibilityLabel = self.customAccessibilityLabel;
  copy.hasAccessoryView = self.hasAccessoryView;
  // LINT.ThenChange(table_view_cell_content_configuration.h:Copy)
  return copy;
}

#pragma mark - UIAccessibility

- (NSString*)accessibilityLabel {
  if (self.customAccessibilityLabel) {
    return self.customAccessibilityLabel;
  }
  NSMutableArray* parts = [NSMutableArray array];

  if (self.attributedTitle.length > 0) {
    [parts addObject:self.attributedTitle.string];
  } else if (self.title.length > 0) {
    [parts addObject:self.title];
  }
  if (self.attributedSubtitle.length > 0) {
    [parts addObject:self.attributedSubtitle.string];
  } else if (self.subtitle.length > 0) {
    [parts addObject:self.subtitle];
  }
  if (self.secondSubtitle.length > 0) {
    [parts addObject:self.secondSubtitle];
  }
  return [parts componentsJoinedByString:@", "];
}

- (NSArray<NSString*>*)accessibilityUserInputLabels {
  if (!self.title) {
    return [super accessibilityUserInputLabels];
  }
  return @[ self.title ];
}

- (NSString*)accessibilityHint {
  NSMutableArray* parts = [NSMutableArray array];

  if (self.leadingConfiguration.accessibilityHint) {
    [parts addObject:self.leadingConfiguration.accessibilityHint];
  }
  if (self.trailingConfiguration.accessibilityHint) {
    [parts addObject:self.trailingConfiguration.accessibilityHint];
  }

  if (parts.count > 0) {
    return [parts componentsJoinedByString:@", "];
  }
  return [super accessibilityHint];
}

- (NSString*)accessibilityValue {
  NSMutableArray* parts = [NSMutableArray array];

  if (self.attributedTrailingText.length > 0) {
    [parts addObject:self.attributedTrailingText.string];
  } else if (self.trailingText.length > 0) {
    [parts addObject:self.trailingText];
  }

  if (self.leadingConfiguration.accessibilityValue) {
    [parts addObject:self.leadingConfiguration.accessibilityValue];
  }
  if (self.trailingConfiguration.accessibilityValue) {
    [parts addObject:self.trailingConfiguration.accessibilityValue];
  }

  if (parts.count > 0) {
    return [parts componentsJoinedByString:@", "];
  }
  return [super accessibilityValue];
}

@end
