// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell_content_configuration.h"

#import "ios/chrome/browser/shared/ui/table_view/cells/legacy_table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_cell_content_view.h"

@implementation TableViewCellContentConfiguration

- (instancetype)init {
  self = [super init];
  if (self) {
    _titleNumberOfLines = 0;
    _subtitleNumberOfLines = 0;
    _trailingTextNumberOfLines = 1;
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
  copy.title = self.title;
  copy.titleColor = self.titleColor;
  copy.titleNumberOfLines = self.titleNumberOfLines;
  copy.leadingConfiguration = [self.leadingConfiguration copyWithZone:zone];
  copy.subtitle = self.subtitle;
  copy.subtitleColor = self.subtitleColor;
  copy.subtitleNumberOfLines = self.subtitleNumberOfLines;
  copy.trailingText = self.trailingText;
  copy.trailingTextColor = self.trailingTextColor;
  copy.trailingTextNumberOfLines = self.trailingTextNumberOfLines;
  // LINT.ThenChange(table_view_cell_content_configuration.h:Copy)
  return copy;
}

#pragma mark - UIAccessibility

- (NSString*)accessibilityLabel {
  NSMutableArray* parts = [NSMutableArray array];

  if (self.title.length > 0) {
    [parts addObject:self.title];
  }
  if (self.subtitle.length > 0) {
    [parts addObject:self.subtitle];
  }
  if (self.trailingText.length > 0) {
    [parts addObject:self.trailingText];
  }

  return [parts componentsJoinedByString:@", "];
}

@end
