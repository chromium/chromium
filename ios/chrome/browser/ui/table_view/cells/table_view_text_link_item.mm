// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_text_link_item.h"

#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/util/label_link_controller.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/string_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

- (void)configureCell:(TableViewCell*)tableCell
           withStyler:(ChromeTableViewStyler*)styler {
  [super configureCell:tableCell withStyler:styler];
  TableViewTextLinkCell* cell =
      base::mac::ObjCCastStrict<TableViewTextLinkCell>(tableCell);
  cell.textLabel.text = self.text;
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  if (!self.linkURL.is_empty())
    [cell setLinkURL:self.linkURL];
}

@end

#pragma mark - TableViewTextLinkCell

@interface TableViewTextLinkCell ()
// Array that holds all LabelLinkController for this Cell.
@property(nonatomic, strong)
    NSMutableArray<LabelLinkController*>* labelLinkControllers;

@end

@implementation TableViewTextLinkCell
@synthesize delegate = _delegate;
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
        [UIFont preferredFontForTextStyle:kTableViewSublabelFontStyle];
    _textLabel.textColor = UIColor.cr_secondaryLabelColor;

    // Add subviews to View Hierarchy.
    [self.contentView addSubview:_textLabel];

    // Create labelLinkController array.
    self.labelLinkControllers = [NSMutableArray array];

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
  LabelLinkController* labelLinkController =
      [self labelLinkControllerForURL:URL];

  // Remove link delimiter from text and get ranges for links. Must be parsed
  // before being added to the controller because modifying the label text
  // clears all added links.
  NSRange otherBrowsingDataRange;
  if (URL.is_valid()) {
    self.textLabel.text =
        ParseStringWithLink(self.textLabel.text, &otherBrowsingDataRange);
    DCHECK(otherBrowsingDataRange.location != NSNotFound &&
           otherBrowsingDataRange.length);
    [labelLinkController addLinkWithRange:otherBrowsingDataRange url:URL];
  }
  [self.labelLinkControllers addObject:labelLinkController];
}

- (void)setLinkURL:(const GURL&)URL forRange:(NSRange)range {
  LabelLinkController* labelLinkController =
      [self labelLinkControllerForURL:URL];
  if (URL.is_valid()) {
    [labelLinkController addLinkWithRange:range url:URL];
  }
  [self.labelLinkControllers addObject:labelLinkController];
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.labelLinkControllers = [NSMutableArray array];
  self.delegate = nil;
}

// Returns a configured labelLinkController.
- (LabelLinkController*)labelLinkControllerForURL:(const GURL&)URL {
  __weak TableViewTextLinkCell* weakSelf = self;
  LabelLinkController* labelLinkController = [[LabelLinkController alloc]
      initWithLabel:self.textLabel
             action:^(const GURL& URL) {
               [[weakSelf delegate] tableViewTextLinkCell:weakSelf
                                        didRequestOpenURL:URL];
             }];
  [labelLinkController setLinkColor:[UIColor colorNamed:kBlueColor]];
  return labelLinkController;
}

@end
