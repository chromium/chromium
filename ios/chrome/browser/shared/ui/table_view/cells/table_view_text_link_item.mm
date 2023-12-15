// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_link_item.h"

#import "base/apple/foundation_util.h"
#import "base/ios/ns_range.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/ui/table_view/legacy_chrome_table_view_styler.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/string_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/table_view/table_view_cells_constants.h"
#import "ios/chrome/common/ui/util/text_view_util.h"

#pragma mark - TableViewTextLinkItem

@implementation TableViewTextLinkItem

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
      base::apple::ObjCCastStrict<TableViewTextLinkCell>(tableCell);
  [cell setText:self.text linkURLs:self.linkURLs linkRanges:self.linkRanges];
  cell.selectionStyle = UITableViewCellSelectionStyleNone;
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
    _textView = CreateUITextViewWithTextKit1();
    _textView.scrollEnabled = NO;
    _textView.editable = NO;
    _textView.delegate = self;
    _textView.translatesAutoresizingMaskIntoConstraints = NO;
    _textView.font = [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _textView.textColor = [UIColor colorNamed:kTextSecondaryColor];
    _textView.linkTextAttributes =
        @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]};
    _textView.backgroundColor = UIColor.clearColor;

    // Add subviews to View Hierarchy.
    [self.contentView addSubview:_textView];

    // Set and activate constraints.
    [NSLayoutConstraint activateConstraints:@[
      // Title Label Constraints.
      [_textView.leadingAnchor
          constraintEqualToAnchor:self.contentView.leadingAnchor
                         constant:kTableViewHorizontalSpacing],
      [_textView.topAnchor constraintEqualToAnchor:self.contentView.topAnchor],
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

- (void)setText:(NSString*)text
       linkURLs:(std::vector<GURL>)linkURLs
     linkRanges:(NSArray*)linkRanges {
  NSDictionary* textAttributes = @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor]
  };

  NSMutableAttributedString* attributedText =
      [[NSMutableAttributedString alloc] initWithString:text
                                             attributes:textAttributes];
  if (linkRanges) {
    [linkRanges enumerateObjectsUsingBlock:^(NSValue* rangeValue, NSUInteger i,
                                             BOOL* stop) {
      CrURL* crurl = [[CrURL alloc] initWithGURL:linkURLs[i]];
      if (!crurl || !crurl.gurl.is_valid()) {
        return;
      }
      [attributedText addAttribute:NSLinkAttributeName
                             value:crurl.nsurl
                             range:rangeValue.rangeValue];
    }];
  }
  self.textView.attributedText = attributedText;
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
  CrURL* crurl = [[CrURL alloc] initWithNSURL:URL];
  [self.delegate tableViewTextLinkCell:self didRequestOpenURL:crurl];
  // Returns NO as the app is handling the opening of the URL.
  return NO;
}

@end
