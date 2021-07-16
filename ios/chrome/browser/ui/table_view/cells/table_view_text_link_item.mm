// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_text_link_item.h"

#include "base/ios/ns_range.h"
#include "base/mac/foundation_util.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "net/base/mac/url_conversions.h"
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
  cell.textView.text = self.text;
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  if (!self.linkURL.is_empty())
    [cell setLinkURL:self.linkURL];
}

@end

#pragma mark - TableViewTextLinkCell

@interface TableViewTextLinkCell () <UITextViewDelegate>
@end

@implementation TableViewTextLinkCell

- (instancetype)initWithStyle:(UITableViewCellStyle)style
              reuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithStyle:style reuseIdentifier:reuseIdentifier];
  if (self) {
    // Text Label, set font sizes using dynamic type.
    _textView = [[UITextView alloc] init];
    _textView.scrollEnabled = NO;
    _textView.editable = NO;
    _textView.delegate = self;
    _textView.translatesAutoresizingMaskIntoConstraints = NO;
    _textView.font =
        [UIFont preferredFontForTextStyle:kTableViewSublabelFontStyle];
    _textView.textColor = UIColor.cr_secondaryLabelColor;

    // Add subviews to View Hierarchy.
    [self.contentView addSubview:_textView];

    // Set and activate constraints.
    [NSLayoutConstraint activateConstraints:@[
      // Title Label Constraints.
      [_textView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_textView.topAnchor
          constraintEqualToAnchor:self.contentView.topAnchor
                         constant:kTableViewLabelVerticalTopSpacing],
      [_textView.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:0],
      [_textView.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kTableViewHorizontalSpacing]
    ]];
  }
  return self;
}

- (void)setLinkURL:(const GURL&)URL {
  if (URL.is_valid()) {
    UITextView* textView = self.textView;
    DCHECK(textView.text.length > 0);
    // Attribute form of the font/color given to the text view on init.
    NSDictionary<NSAttributedStringKey, id>* textAttributes =
        [textView.attributedText attributesAtIndex:0 effectiveRange:nullptr];
    textView.attributedText = AttributedStringFromStringWithLink(
        textView.text, textAttributes, [self linkAttributesForURL:URL]);
  }
}

- (void)setLinkURL:(const GURL&)URL forRange:(NSRange)range {
  if (URL.is_valid()) {
    NSMutableAttributedString* text = [[NSMutableAttributedString alloc]
        initWithAttributedString:self.textView.attributedText];
    [text addAttributes:[self linkAttributesForURL:URL] range:range];
    self.textView.attributedText = text;
  }
}

- (NSDictionary<NSAttributedStringKey, id>*)linkAttributesForURL:
    (const GURL&)URL {
  NSURL* linkURL = net::NSURLWithGURL(URL);
  return @{
    NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor],
    NSLinkAttributeName : linkURL,
  };
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.delegate = nil;
}

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  DCHECK(self.textView == textView);
  DCHECK(URL);
  [self.delegate tableViewTextLinkCell:self
                     didRequestOpenURL:net::GURLWithNSURL(URL)];
  // Returns NO as the app is handling the opening of the URL.
  return NO;
}

@end
