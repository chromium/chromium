// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_link_header_footer_item.h"

#import "base/check_op.h"
#import "base/containers/contains.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/public/features/features.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/text_view_util.h"
#import "net/base/apple/url_conversions.h"

namespace {

// Padding used on the top and bottom edges of the cell.
const CGFloat kVerticalPadding = 8;

// Horizontal padding used to align the header/footer with the section items.
const CGFloat kHorizontalSpacingToAlignWithItems = 16.0;

}  // namespace

@implementation TableViewLinkHeaderFooterItem {
  NSArray<CrURL*>* urls_;
}

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewLinkHeaderFooterView class];
  }
  return self;
}

#pragma mark Properties

- (NSArray<CrURL*>*)urls {
  return urls_;
}

- (void)setUrls:(NSArray<CrURL*>*)urls {
  for (CrURL* url in urls_) {
    DCHECK(url.gurl.is_valid());
  }
  urls_ = urls;
}

#pragma mark CollectionViewItem

- (void)configureHeaderFooterView:(TableViewLinkHeaderFooterView*)headerFooter
                       withStyler:(ChromeTableViewStyler*)styler {
  [super configureHeaderFooterView:headerFooter withStyler:styler];

  if ([self.urls count] != 0) {
    headerFooter.urls = self.urls;
  }

  if (self.forceIndents) {
    [headerFooter setForceIndents:YES];
  }

  UIColor* textColor = self.textColor
                           ? self.textColor
                           : [UIColor colorNamed:kTextSecondaryColor];
  [headerFooter setText:self.text withColor:textColor];
}

@end

@interface TableViewLinkHeaderFooterView () <UITextViewDelegate>

// UITextView corresponding to `text` from the item.
@property(nonatomic, readonly, strong) UITextView* textView;

@end

@implementation TableViewLinkHeaderFooterView {
  NSArray<CrURL*>* urls_;
  // Leading constaint for item.
  NSLayoutConstraint* leadingConstraint_;
  // Trailing constraint for item.
  NSLayoutConstraint* trailingConstraint_;
}

@synthesize textView = _textView;

- (instancetype)initWithReuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithReuseIdentifier:reuseIdentifier];
  if (self) {
    urls_ = @[];
    _textView = CreateUITextViewWithTextKit1();
    _textView.scrollEnabled = NO;
    _textView.editable = NO;
    _textView.delegate = self;
    _textView.backgroundColor = UIColor.clearColor;
    _textView.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _textView.adjustsFontForContentSizeCategory = YES;
    _textView.translatesAutoresizingMaskIntoConstraints = NO;
    _textView.linkTextAttributes =
        @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]};
    _textView.textContainer.lineFragmentPadding = 0;
    _textView.textContainerInset = UIEdgeInsetsZero;

    [self.contentView addSubview:_textView];

    leadingConstraint_ = [_textView.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:HorizontalPadding()];
    trailingConstraint_ = [_textView.trailingAnchor
        constraintEqualToAnchor:self.contentView.trailingAnchor
                       constant:-HorizontalPadding()];

    [NSLayoutConstraint activateConstraints:@[
      [_textView.topAnchor constraintEqualToAnchor:self.contentView.topAnchor
                                          constant:kVerticalPadding],
      [_textView.bottomAnchor
          constraintEqualToAnchor:self.contentView.bottomAnchor
                         constant:-kVerticalPadding],
      trailingConstraint_,
      leadingConstraint_,
    ]];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  self.textView.text = nil;
  self.textView.selectable = YES;
  self.textView.linkTextAttributes =
      @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]};
  self.delegate = nil;
  self.urls = @[];
  self.forceIndents = NO;
}

#pragma mark - Properties

- (void)setText:(NSString*)text withColor:(UIColor*)color {
  [self setText:text withColor:color textAlignment:NSTextAlignmentNatural];
}

- (void)setText:(NSString*)text
        withColor:(UIColor*)color
    textAlignment:(NSTextAlignment)textAlignment {
  StringWithTags parsedString = ParseStringWithLinks(text);
  NSMutableParagraphStyle* paragraphStyle =
      [[NSMutableParagraphStyle alloc] init];
  paragraphStyle.alignment = textAlignment;

  NSDictionary* textAttributes = @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
    NSForegroundColorAttributeName : color,
    NSParagraphStyleAttributeName : paragraphStyle
  };

  NSMutableAttributedString* attributedText =
      [[NSMutableAttributedString alloc] initWithString:parsedString.string
                                             attributes:textAttributes];

  DCHECK_EQ(parsedString.ranges.size(), [self.urls count]);
  size_t index = 0;
  for (CrURL* url in self.urls) {
    [attributedText addAttribute:NSLinkAttributeName
                           value:url.nsurl
                           range:parsedString.ranges[index]];
    index += 1;
  }

  self.textView.attributedText = attributedText;
}

- (NSArray<CrURL*>*)urls {
  return urls_;
}

- (void)setUrls:(NSArray<CrURL*>*)urls {
  for (CrURL* url in urls_) {
    DCHECK(url.gurl.is_valid());
  }
  urls_ = urls;
}

- (void)setForceIndents:(BOOL)forceIndents {
  leadingConstraint_.constant =
      forceIndents ? kHorizontalSpacingToAlignWithItems : HorizontalPadding();
  trailingConstraint_.constant =
      forceIndents ? -kHorizontalSpacingToAlignWithItems : -HorizontalPadding();
}

- (void)setLinkEnabled:(BOOL)enabled {
  self.textView.selectable = enabled;
  _textView.linkTextAttributes = @{
    NSForegroundColorAttributeName :
        [UIColor colorNamed:enabled ? kBlueColor : kDisabledTintColor]
  };
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  DCHECK(self.textView == textView);
  CrURL* crurl = [[CrURL alloc] initWithNSURL:URL];
  DCHECK(crurl.gurl.is_valid());
  // DCHECK(base::Contains(self.urls, gURL));
  [self.delegate view:self didTapLinkURL:crurl];
  // Returns NO as the app is handling the opening of the URL.
  return NO;
}

- (void)textViewDidChangeSelection:(UITextView*)textView {
  // Always force the `selectedTextRange` to `nil` to prevent users from
  // selecting text. Setting the `selectable` property to `NO` doesn't help
  // since it makes links inside the text view untappable.
  textView.selectedTextRange = nil;
}

@end
