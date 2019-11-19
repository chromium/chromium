// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"

#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/string_util.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Padding used on the leading and trailing edges of the cell.
const CGFloat kHorizontalPadding = 24;

// Padding used on the top and bottom edges of the cell.
const CGFloat kVerticalPadding = 8;

}  // namespace

@implementation TableViewLinkHeaderFooterItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewLinkHeaderFooterView class];
  }
  return self;
}

#pragma mark CollectionViewItem

- (void)configureHeaderFooterView:(TableViewLinkHeaderFooterView*)headerFooter
                       withStyler:(ChromeTableViewStyler*)styler {
  [super configureHeaderFooterView:headerFooter withStyler:styler];

  headerFooter.linkURL = self.linkURL;
  if (self.linkURL.is_valid())
    headerFooter.accessibilityTraits |= UIAccessibilityTraitLink;
  else
    headerFooter.accessibilityTraits &= ~UIAccessibilityTraitLink;
  [headerFooter setText:self.text];
}

@end

@interface TableViewLinkHeaderFooterView ()<UITextViewDelegate>

// UITextView corresponding to |text| from the item.
@property(nonatomic, readonly, strong) UITextView* textView;

@end

@implementation TableViewLinkHeaderFooterView

@synthesize textView = _textView;

- (instancetype)initWithReuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithReuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = YES;

    _textView = [[UITextView alloc] init];
    _textView.scrollEnabled = NO;
    _textView.editable = NO;
    _textView.delegate = self;
    _textView.backgroundColor = UIColor.clearColor;
    _textView.font =
        [UIFont preferredFontForTextStyle:kTableViewSublabelFontStyle];
    _textView.adjustsFontForContentSizeCategory = YES;
    _textView.translatesAutoresizingMaskIntoConstraints = NO;
    _textView.linkTextAttributes =
        @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]};

    [self.contentView addSubview:_textView];

    [NSLayoutConstraint activateConstraints:@[
      [_textView.topAnchor constraintEqualToAnchor:self.contentView.topAnchor
                                          constant:kVerticalPadding],
      [_textView.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:-kVerticalPadding],
      [_textView.trailingAnchor
          constraintEqualToAnchor:self.contentView.trailingAnchor
                         constant:-kHorizontalPadding],
      [_textView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kHorizontalPadding],
    ]];
  }
  return self;
}

- (void)setText:(NSString*)text {
  NSRange range;

  NSString* strippedText = ParseStringWithLink(text, &range);
  NSRange fullRange = NSMakeRange(0, strippedText.length);
  NSMutableAttributedString* attributedText =
      [[NSMutableAttributedString alloc] initWithString:strippedText];
  [attributedText addAttribute:NSForegroundColorAttributeName
                         value:UIColor.cr_secondaryLabelColor
                         range:fullRange];

  [attributedText
      addAttribute:NSFontAttributeName
             value:[UIFont
                       preferredFontForTextStyle:kTableViewSublabelFontStyle]
             range:fullRange];

  if (range.location != NSNotFound && range.length != 0) {
    NSURL* URL = net::NSURLWithGURL(self.linkURL);
    id linkValue = URL ? URL : @"";
    [attributedText addAttribute:NSLinkAttributeName
                           value:linkValue
                           range:range];
  }

  self.textView.attributedText = attributedText;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.textView.text = nil;
  self.delegate = nil;
  self.linkURL = GURL();
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  DCHECK(self.textView == textView);
  GURL convertedURL = URL ? net::GURLWithNSURL(URL) : self.linkURL;
  [self.delegate view:self didTapLinkURL:convertedURL];
  // Returns NO as the app is handling the opening of the URL.
  return NO;
}

#pragma mark - NSObject(Accessibility)

- (NSString*)accessibilityLabel {
  return [self.textView.attributedText string];
}

@end
