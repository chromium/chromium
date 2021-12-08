// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_attributed_header_footer_item.h"

#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Padding used on the top and bottom edges of the cell.
const CGFloat kVerticalPadding = 8;

}  // namespace

@implementation TableViewAttributedHeaderFooterItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewAttributedHeaderFooterView class];
  }
  return self;
}

#pragma mark CollectionViewItem

- (void)configureHeaderFooterView:
            (TableViewAttributedHeaderFooterView*)headerFooter
                       withStyler:(ChromeTableViewStyler*)styler {
  [super configureHeaderFooterView:headerFooter withStyler:styler];

  [headerFooter setText:self.text
      withCustomTextAttributes:self.customTextAttributesOnRange];
}

@end

@interface TableViewAttributedHeaderFooterView ()

// UILabel corresponding to |text| from the item.
@property(nonatomic, readonly, strong) UILabel* textLabel;

@end

@implementation TableViewAttributedHeaderFooterView

@synthesize textLabel = _textLabel;

- (instancetype)initWithReuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithReuseIdentifier:reuseIdentifier];
  if (self) {
    _textLabel = [[UILabel alloc] init];
    _textLabel.backgroundColor = UIColor.clearColor;
    _textLabel.font =
        [UIFont preferredFontForTextStyle:kTableViewSublabelFontStyle];

    _textLabel.adjustsFontForContentSizeCategory = YES;
    _textLabel.translatesAutoresizingMaskIntoConstraints = NO;

    [self.contentView addSubview:_textLabel];

    [NSLayoutConstraint activateConstraints:@[
      [_textLabel.topAnchor constraintEqualToAnchor:self.contentView.topAnchor
                                           constant:kVerticalPadding],
      [_textLabel.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:-kVerticalPadding],
      [_textLabel.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-HorizontalPadding()],
      [_textLabel.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:HorizontalPadding()],
    ]];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.textLabel.text = nil;
  self.textLabel.attributedText = nil;
}

#pragma mark - Properties

- (void)setText:(NSString*)text
    withCustomTextAttributes:
        (NSDictionary<NSValue*, NSDictionary*>*)customTextAttributesOnRange {
  NSDictionary* textAttributes = @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:kTableViewSublabelFontStyle],
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor]
  };

  NSMutableAttributedString* attributedText =
      [[NSMutableAttributedString alloc] initWithString:text
                                             attributes:textAttributes];

  if ([customTextAttributesOnRange count] > 0) {
    [customTextAttributesOnRange
        enumerateKeysAndObjectsUsingBlock:^(NSValue* range, NSDictionary* attrs,
                                            BOOL* stop) {
          [attributedText setAttributes:attrs range:range.rangeValue];
        }];
  }

  self.textLabel.attributedText = attributedText;
}

@end
