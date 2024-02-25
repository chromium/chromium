// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/whats_new/cells/whats_new_table_view_fake_header_item.h"

#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"

namespace {

// The size of the leading margin between content view and the text labels.
const CGFloat kItemLeadingMargin = 16.0;
// The size of the top margin.
const CGFloat kTopMargin = 24.0;

}  // namespace

#pragma mark - WhatsNewTableViewFakeHeaderItem

@implementation WhatsNewTableViewFakeHeaderItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [WhatsNewTableViewFakeHeaderCell class];
  }
  return self;
}

#pragma mark - TableViewItem

- (void)configureCell:(WhatsNewTableViewFakeHeaderCell*)cell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:cell withStyler:styler];
  cell.headerLabel.text = self.text;
  cell.accessibilityLabel = self.text;
}

@end

#pragma mark - WhatsNewTableViewFakeHeaderCell

@implementation WhatsNewTableViewFakeHeaderCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;

    self.headerLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    self.headerLabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
    self.headerLabel.font =
        CreateDynamicFont(UIFontTextStyleSubheadline, UIFontWeightSemibold);
    self.headerLabel.adjustsFontForContentSizeCategory = YES;
    self.headerLabel.translatesAutoresizingMaskIntoConstraints = NO;
    self.headerLabel.numberOfLines = 1;
    [self.contentView addSubview:self.headerLabel];

    [NSLayoutConstraint activateConstraints:@[
      [self.headerLabel.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kItemLeadingMargin],
      [self.headerLabel.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kItemLeadingMargin],
      [self.headerLabel.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      [self.headerLabel.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor
                         constant:kTopMargin],
      [self.headerLabel.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor],
    ]];
  }
  return self;
}

#pragma mark - UIReusableView

- (void)prepareForReuse {
  [super prepareForReuse];
  self.headerLabel.text = nil;
}

@end
