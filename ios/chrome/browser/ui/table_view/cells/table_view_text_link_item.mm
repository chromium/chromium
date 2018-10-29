// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_text_link_item.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/util/label_link_controller.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/string_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// The width and height of the favicon ImageView.
const CGFloat kTextCellLinkColor = 0x1A73E8;
}  // namespace

#pragma mark - TableViewTextLinkItem

@implementation TableViewTextLinkItem
@synthesize text = _text;
@synthesize linkURL = _linkURL;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewTextLinkCell class];
  }
  return self;
}

- (void)configureCell:(UITableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];
  TableViewTextLinkCell* cell =
      base::mac::ObjCCastStrict<TableViewTextLinkCell>(tableCell);
  cell.textLabel.text = self.text;
  cell.textLabel.backgroundColor = styler.tableViewBackgroundColor;
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  [cell setLinkURL:self.linkURL];
}

@end

#pragma mark - TableViewTextLinkCell

@interface TableViewTextLinkCell ()
// LabelLinkController that configures the link on the Cell's text.
@property(nonatomic, strong, readwrite)
    LabelLinkController* labelLinkController;
@end

@implementation TableViewTextLinkCell
@synthesize delegate = _delegate;
@synthesize labelLinkController = _labelLinkController;
@synthesize textLabel = _textLabel;

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    // Text Label, set font sizes using dynamic type.
    _textLabel = [[UILabel alloc] init];
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;
    _textLabel.numberOfLines = 0;
    _textLabel.lineBreakMode = NSLineBreakByWordWrapping;
    _textLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _textLabel.textColor = [UIColor grayColor];

    // Add subviews to View Hierarchy.
    [self.contentView addSubview:_textLabel];

    // Set and activate constraints.
    [NSLayoutConstraint activateConstraints:@[
      // Title Label Constraints.
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_textLabel.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor
                         constant:kTableViewLabelVerticalTopSpacing],
      [_textLabel.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:0],
      [_textLabel.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing]
    ]];
  }
  return self;
}

- (void)setLinkURL:(const GURL&)URL {
  // Init and configure the labelLinkController.
  __weak TableViewTextLinkCell* weakSelf = self;
  self.labelLinkController = [[LabelLinkController alloc]
      initWithLabel:self.textLabel
             action:^(const GURL& URL) {
               [[weakSelf delegate] tableViewTextLinkCell:weakSelf
                                        didRequestOpenURL:URL];
             }];
  [self.labelLinkController setLinkColor:UIColorFromRGB(kTextCellLinkColor)];

  // Remove link delimiter from text and get ranges for links. Must be parsed
  // before being added to the controller because modifying the label text
  // clears all added links.
  NSRange otherBrowsingDataRange;
  if (URL.is_valid()) {
    self.textLabel.text =
        ParseStringWithLink(self.textLabel.text, &otherBrowsingDataRange);
    DCHECK(otherBrowsingDataRange.location != NSNotFound &&
           otherBrowsingDataRange.length);
    [self.labelLinkController addLinkWithRange:otherBrowsingDataRange url:URL];
  }
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.labelLinkController = nil;
  self.delegate = nil;
}

@end
