// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_link_header_footer_item.h"

#import "base/check_op.h"
#import "base/containers/contains.h"
#import "ios/chrome/browser/net/crurl.h"
#import "ios/chrome/browser/ui/ui_feature_flags.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/text_view_util.h"
#import "net/base/mac/url_conversions.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Padding used on the top and bottom edges of the cell.
const CGFloat kVerticalPadding = 8;

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
  UIColor* textColor = self.textColor
                           ? self.textColor
                           : [UIColor colorNamed:kTextSecondaryColor];
  [headerFooter setText:self.text withColor:textColor];
}

@end

@interface TableViewLinkHeaderFooterView ()<UITextViewDelegate>

// UITextView corresponding to `text` from the item.
@property(nonatomic, readonly, strong) UITextView* textView;

@end

@implementation TableViewLinkHeaderFooterView {
  NSArray<CrURL*>* urls_;
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

- (void)prepareForReuse {
  [super prepareForReuse];
  self.textView.text = nil;
  self.delegate = nil;
  self.urls = @[];
}

#pragma mark - Properties

- (void)setText:(NSString*)text withColor:(UIColor*)color {
  StringWithTags parsedString = ParseStringWithLinks(text);

  NSDictionary* textAttributes = @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
    NSForegroundColorAttributeName : color
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
