// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_text_header_footer_item.h"

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

// Horizontal padding used to align the header/footer with the section items.
const CGFloat kHorizontalSpacingToAlignWithItems = 16.0;

}  // namespace

@implementation TableViewTextHeaderFooterItem

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewTextHeaderFooterView class];
  }
  return self;
}

#pragma mark Properties

- (void)setURLs:(NSArray<CrURL*>*)URLs {
  for (CrURL* URL in URLs) {
    DCHECK(URL.gurl.is_valid());
  }
  _URLs = URLs;
}

#pragma mark CollectionViewItem

- (void)configureHeaderFooterView:(TableViewTextHeaderFooterView*)headerFooter
                       withStyler:(ChromeTableViewStyler*)styler {
  [super configureHeaderFooterView:headerFooter withStyler:styler];

  if ([self.URLs count] != 0) {
    headerFooter.URLs = self.URLs;
  }

  if (self.forceIndents) {
    [headerFooter setForceIndents:YES];
  }

  [headerFooter setSubtitle:self.subtitle];
  [headerFooter setTitle:self.text];
}

@end

@interface TableViewTextHeaderFooterView () <UITextViewDelegate>

// UITextView corresponding to `subtitle` from the item.
@property(nonatomic, readonly, strong) UITextView* subtitleView;

// The UILabel containing the text stored in `text`.
@property(nonatomic, readonly, strong) UILabel* textLabel;

@end

@implementation TableViewTextHeaderFooterView {
  // Leading constaint for item.
  NSLayoutConstraint* leadingAnchorConstraint_;
  // Trailing constraint for item.
  NSLayoutConstraint* trailingAnchorConstraint_;
}
@synthesize subtitleView = _subtitleView;
@synthesize textLabel = _textLabel;

- (instancetype)initWithReuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithReuseIdentifier:reuseIdentifier];
  if (self) {
    self.isAccessibilityElement = NO;

    _URLs = @[];
    _subtitleView = CreateUITextViewWithTextKit1();
    _subtitleView.scrollEnabled = NO;
    _subtitleView.editable = NO;
    _subtitleView.delegate = self;
    _subtitleView.backgroundColor = UIColor.clearColor;
    _subtitleView.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote];
    _subtitleView.adjustsFontForContentSizeCategory = YES;
    _subtitleView.translatesAutoresizingMaskIntoConstraints = NO;
    _subtitleView.linkTextAttributes =
        @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]};
    _subtitleView.textAlignment = NSTextAlignmentLeft;
    _subtitleView.textContainer.lineFragmentPadding = 0;
    _subtitleView.textContainerInset = UIEdgeInsetsZero;
    _subtitleView.hidden = YES;

    // Labels, set font sizes using dynamic type.
    _textLabel = [[UILabel alloc] init];
    _textLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleHeadline];
    _textLabel.accessibilityTraits = UIAccessibilityTraitHeader;
    _textLabel.numberOfLines = 0;

    // Vertical StackView.
    UIStackView* verticalStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _textLabel, _subtitleView ]];
    verticalStack.axis = UILayoutConstraintAxisVertical;
    verticalStack.translatesAutoresizingMaskIntoConstraints = NO;

    // Container View.
    UIView* containerView = [[UIView alloc] init];
    containerView.translatesAutoresizingMaskIntoConstraints = NO;

    // Add subviews to View Hierarchy.
    [containerView addSubview:verticalStack];
    [self.contentView addSubview:containerView];

    // Lower the padding constraints priority. UITableView might try to set
    // the header view height/width to 0 breaking the constraints. See
    // https://crbug.com/854117 for more information.
    NSLayoutConstraint* heightConstraint =
        [self.contentView.heightAnchor constraintGreaterThanOrEqualToConstant:
                                           kTableViewHeaderFooterViewHeight];
    // Set this constraint to UILayoutPriorityDefaultHigh + 1 in order to guard
    // against some elements that might UILayoutPriorityDefaultHigh to expand
    // beyond the margins.
    heightConstraint.priority = UILayoutPriorityDefaultHigh + 1;
    NSLayoutConstraint* topAnchorConstraint = [containerView.topAnchor
        constraintEqualToAnchor:self.contentView.topAnchor
                       constant:kTableViewVerticalSpacing];
    topAnchorConstraint.priority = UILayoutPriorityDefaultHigh;
    NSLayoutConstraint* bottomAnchorConstraint = [containerView.bottomAnchor
        constraintEqualToAnchor:self.contentView.bottomAnchor
                       constant:-kTableViewVerticalSpacing];
    bottomAnchorConstraint.priority = UILayoutPriorityDefaultHigh;
    leadingAnchorConstraint_ = [containerView.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:HorizontalPadding()];
    leadingAnchorConstraint_.priority = UILayoutPriorityDefaultHigh;
    trailingAnchorConstraint_ = [containerView.trailingAnchor
        constraintEqualToAnchor:self.contentView.trailingAnchor
                       constant:-HorizontalPadding()];
    trailingAnchorConstraint_.priority = UILayoutPriorityDefaultHigh;

    // Set and activate constraints.
    [NSLayoutConstraint activateConstraints:@[
      // Container Constraints.
      heightConstraint,
      topAnchorConstraint,
      bottomAnchorConstraint,
      leadingAnchorConstraint_,
      trailingAnchorConstraint_,
      [containerView.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor],
      // Vertical StackView Constraints.
      [verticalStack.leadingAnchor
          constraintEqualToAnchor:containerView.leadingAnchor],
      [verticalStack.topAnchor constraintEqualToAnchor:containerView.topAnchor],
      [verticalStack.bottomAnchor
          constraintEqualToAnchor:containerView.bottomAnchor],
      [verticalStack.trailingAnchor
          constraintEqualToAnchor:containerView.trailingAnchor],
    ]];
  }
  return self;
}

- (void)prepareForReuse {
  [super prepareForReuse];
  [self setTitle:nil];
  [self setSubtitle:nil];
  self.delegate = nil;
  self.URLs = @[];
  self.forceIndents = NO;
}

#pragma mark - Properties

- (void)setTitle:(NSString*)title {
  self.textLabel.text = title;
}

- (void)setSubtitle:(NSString*)subtitle {
  [self setSubtitle:subtitle withColor:nil];
}

- (void)setSubtitle:(NSString*)subtitle withColor:(UIColor*)color {
  if (!subtitle) {
    // If no subtitle, hide the subtitle view to avoid taking space for nothing.
    self.subtitleView.hidden = YES;
    return;
  }
  // Else, ensure the subtitle view is visible.
  self.subtitleView.hidden = NO;

  StringWithTags parsedString = ParseStringWithLinks(subtitle);

  UIColor* textColor = color ? color : [UIColor colorNamed:kTextSecondaryColor];

  NSDictionary* textAttributes = @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
    NSForegroundColorAttributeName : textColor
  };

  NSMutableAttributedString* attributedText =
      [[NSMutableAttributedString alloc] initWithString:parsedString.string
                                             attributes:textAttributes];

  DCHECK_EQ(parsedString.ranges.size(), [self.URLs count]);
  size_t index = 0;
  for (CrURL* URL in self.URLs) {
    [attributedText addAttribute:NSLinkAttributeName
                           value:URL.nsurl
                           range:parsedString.ranges[index]];
    index += 1;
  }

  self.subtitleView.attributedText = attributedText;
}

- (void)setForceIndents:(BOOL)forceIndents {
  leadingAnchorConstraint_.constant =
      forceIndents ? kHorizontalSpacingToAlignWithItems : HorizontalPadding();
  trailingAnchorConstraint_.constant =
      forceIndents ? -kHorizontalSpacingToAlignWithItems : -HorizontalPadding();
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  DCHECK(self.subtitleView == textView);
  CrURL* crurl = [[CrURL alloc] initWithNSURL:URL];
  DCHECK(crurl.gurl.is_valid());

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
