// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"

#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#include "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/UIColor+cr_semantic_colors.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Padding used on the leading and trailing edges of the cell.
const CGFloat kHorizontalPadding = 16;

// Padding used on the top and bottom edges of the cell.
const CGFloat kVerticalPadding = 8;

// Returns a padding according to the width of the current device.
CGFloat HorizontalPadding() {
  if (base::FeatureList::IsEnabled(kSettingsRefresh) && !IsSmallDevice())
    return 0;
  return kHorizontalPadding;
}

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
                         constant:-HorizontalPadding()],
      [_textView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:HorizontalPadding()],
    ]];
  }
  return self;
}

- (void)setText:(NSString*)text {
  NSDictionary* textAttributes = @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:kTableViewSublabelFontStyle],
    NSForegroundColorAttributeName : UIColor.cr_secondaryLabelColor
  };

  NSAttributedString* attributedText;
  if (self.linkURL.is_empty()) {
    attributedText =
        [[NSMutableAttributedString alloc] initWithString:text
                                               attributes:textAttributes];
  } else {
    NSDictionary* linkAttributes =
        @{NSLinkAttributeName : net::NSURLWithGURL(self.linkURL)};
    attributedText = AttributedStringFromStringWithLink(text, textAttributes,
                                                        linkAttributes);
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
