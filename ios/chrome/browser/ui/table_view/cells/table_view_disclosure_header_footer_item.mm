// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/table_view/cells/table_view_disclosure_header_footer_item.h"

#include "base/mac/foundation_util.h"
#include "base/numerics/math_constants.h"
#import "ios/chrome/browser/ui/table_view/cells/table_view_cells_constants.h"
#import "ios/chrome/browser/ui/table_view/chrome_table_view_styler.h"
#import "ios/chrome/browser/ui/util/rtl_geometry.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/UIColor+cr_semantic_colors.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ui/base/l10n/l10n_util_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Identity rotation angle that positions disclosure pointing down.
constexpr float kRotationNinetyCW = (90 / 180.0) * M_PI;
}

@implementation TableViewDisclosureHeaderFooterItem
@synthesize subtitleText = _subtitleText;
@synthesize text = _text;
@synthesize collapsed = _collapsed;

- (instancetype)initWithType:(NSInteger)type {
  self = [super initWithType:type];
  if (self) {
    self.cellClass = [TableViewDisclosureHeaderFooterView class];
  }
  return self;
}

- (void)configureHeaderFooterView:(UITableViewHeaderFooterView*)headerFooter
                       withStyler:(ChromeTableViewStyler*)styler {
  [super configureHeaderFooterView:headerFooter withStyler:styler];
  TableViewDisclosureHeaderFooterView* header =
      base::mac::ObjCCastStrict<TableViewDisclosureHeaderFooterView>(
          headerFooter);
  header.titleLabel.text = self.text;
  header.subtitleLabel.text = self.subtitleText;
  header.isAccessibilityElement = YES;
  header.accessibilityTraits |= UIAccessibilityTraitButton;
  DisclosureDirection direction =
      self.collapsed ? DisclosureDirectionTrailing : DisclosureDirectionDown;
  [header setInitialDirection:direction];
  if (styler.headerFooterTitleColor)
    header.titleLabel.textColor = styler.headerFooterTitleColor;
  if (styler.headerFooterDetailColor)
    header.subtitleLabel.textColor = styler.headerFooterDetailColor;
  if (styler.cellHighlightColor)
    header.highlightColor = styler.cellHighlightColor;
}

@end

#pragma mark - TableViewDisclosureHeaderFooterView

@interface TableViewDisclosureHeaderFooterView ()
// Animator that handles all cell animations.
@property(strong, nonatomic) UIViewPropertyAnimator* cellAnimator;
// ImageView that holds the disclosure accessory icon.
@property(strong, nonatomic) UIImageView* disclosureImageView;
// The cell's default color at the moment of starting the highlight animation,
// if no color is set defaults to clearColor.
@property(strong, nonatomic) UIColor* cellDefaultBackgroundColor;
@end

@implementation TableViewDisclosureHeaderFooterView
@synthesize cellAnimator = _cellAnimator;
@synthesize cellDefaultBackgroundColor = _cellDefaultBackgroundColor;
@synthesize disclosureDirection = disclosureDirection;
@synthesize disclosureImageView = _disclosureImageView;
@synthesize highlightColor = _highlightColor;
@synthesize subtitleLabel = _subtitleLabel;
@synthesize titleLabel = _titleLabel;

- (instancetype)initWithReuseIdentifier:(NSString*)reuseIdentifier {
  self = [super initWithReuseIdentifier:reuseIdentifier];
  if (self) {
    // Labels, set font sizes using dynamic type.
    _titleLabel = [[UILabel alloc] init];
    UIFontDescriptor* baseDescriptor = [UIFontDescriptor
        preferredFontDescriptorWithTextStyle:UIFontTextStyleSubheadline];
    UIFontDescriptor* styleDescriptor = [baseDescriptor
        fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
    _titleLabel.font =
        [UIFont fontWithDescriptor:styleDescriptor size:kUseDefaultFontSize];
    [_titleLabel
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:UILayoutConstraintAxisVertical];

    _subtitleLabel = [[UILabel alloc] init];
    _subtitleLabel.font =
        [UIFont preferredFontForTextStyle:UIFontTextStyleCaption1];
    _subtitleLabel.textColor = UIColor.cr_secondaryLabelColor;
    [_subtitleLabel
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:UILayoutConstraintAxisVertical];

    // Vertical StackView.
    UIStackView* verticalStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ _titleLabel, _subtitleLabel ]];
    verticalStack.axis = UILayoutConstraintAxisVertical;

    // Disclosure ImageView. Initial pointing direction is to the right.
    _disclosureImageView = [[UIImageView alloc]
        initWithImage:[UIImage imageNamed:@"table_view_cell_chevron"]];
    [_disclosureImageView
        setContentHuggingPriority:UILayoutPriorityDefaultHigh
                          forAxis:UILayoutConstraintAxisHorizontal];

    // Horizontal StackView.
    UIStackView* horizontalStack = [[UIStackView alloc]
        initWithArrangedSubviews:@[ verticalStack, _disclosureImageView ]];
    horizontalStack.axis = UILayoutConstraintAxisHorizontal;
    horizontalStack.spacing = kTableViewSubViewHorizontalSpacing;
    horizontalStack.translatesAutoresizingMaskIntoConstraints = NO;
    horizontalStack.alignment = UIStackViewAlignmentCenter;

    // Add subviews to View Hierarchy.
    [self.contentView addSubview:horizontalStack];

    // Lower the padding constraints priority. UITableView might try to set
    // the header view height/width to 0 breaking the constraints. See
    // https://crbug.com/854117 for more information.
    NSLayoutConstraint* topAnchorConstraint = [horizontalStack.topAnchor
        constraintGreaterThanOrEqualToAnchor:self.contentView.topAnchor
                                    constant:kTableViewVerticalSpacing];
    topAnchorConstraint.priority = UILayoutPriorityDefaultHigh;
    NSLayoutConstraint* bottomAnchorConstraint = [horizontalStack.bottomAnchor
        constraintLessThanOrEqualToAnchor:self.contentView.bottomAnchor
                                 constant:-kTableViewVerticalSpacing];
    bottomAnchorConstraint.priority = UILayoutPriorityDefaultHigh;
    NSLayoutConstraint* leadingAnchorConstraint = [horizontalStack.leadingAnchor
        constraintEqualToAnchor:self.contentView.leadingAnchor
                       constant:kTableViewHorizontalSpacing];
    leadingAnchorConstraint.priority = UILayoutPriorityDefaultHigh;
    NSLayoutConstraint* trailingAnchorConstraint =
        [horizontalStack.trailingAnchor
            constraintEqualToAnchor:self.contentView.trailingAnchor
                           constant:-kTableViewHorizontalSpacing];
    trailingAnchorConstraint.priority = UILayoutPriorityDefaultHigh;

    // Set and activate constraints.
    [NSLayoutConstraint activateConstraints:@[
      topAnchorConstraint, bottomAnchorConstraint, leadingAnchorConstraint,
      trailingAnchorConstraint,
      [horizontalStack.centerYAnchor
          constraintEqualToAnchor:self.contentView.centerYAnchor]
    ]];
  }
  return self;
}

#pragma mark - View LifeCycle

- (void)prepareForReuse {
  [super prepareForReuse];
  self.cellDefaultBackgroundColor = nil;
  if (self.cellAnimator.isRunning)
    [self.cellAnimator stopAnimation:YES];
}

- (void)traitCollectionDidChange:(UITraitCollection*)previousTraitCollection {
  [super traitCollectionDidChange:previousTraitCollection];
  if (previousTraitCollection.preferredContentSizeCategory !=
      self.traitCollection.preferredContentSizeCategory) {
    UIFontDescriptor* baseDescriptor = [UIFontDescriptor
        preferredFontDescriptorWithTextStyle:UIFontTextStyleHeadline];
    UIFontDescriptor* styleDescriptor = [baseDescriptor
        fontDescriptorWithSymbolicTraits:UIFontDescriptorTraitBold];
    self.titleLabel.font =
        [UIFont fontWithDescriptor:styleDescriptor size:kUseDefaultFontSize];
  }
}

#pragma mark - public methods

- (void)animateHighlight {
  [self addAnimationHighlightToAnimator];
  [self.cellAnimator startAnimation];
}

- (void)setInitialDirection:(DisclosureDirection)direction {
  [self rotateToDirection:direction animate:NO];
}

- (void)animateHighlightAndRotateToDirection:(DisclosureDirection)direction {
  [self addAnimationHighlightToAnimator];
  [self rotateToDirection:direction animate:YES];
  [self.cellAnimator startAnimation];
}

#pragma mark - internal methods

- (void)addAnimationHighlightToAnimator {
  UIColor* originalBackgroundColor = self.cellDefaultBackgroundColor;
  self.cellAnimator = [[UIViewPropertyAnimator alloc]
      initWithDuration:kTableViewCellSelectionAnimationDuration
                 curve:UIViewAnimationCurveLinear
            animations:^{
              self.contentView.backgroundColor = self.highlightColor;
            }];
  __weak TableViewDisclosureHeaderFooterView* weakSelf = self;
  [self.cellAnimator addCompletion:^(UIViewAnimatingPosition finalPosition) {
    weakSelf.contentView.backgroundColor = originalBackgroundColor;
  }];
}

// When view is being initialized, it has not been added to the hierarchy yet.
// So, in order to set the initial direction, a non-animation transform is
// needed.
- (void)rotateToDirection:(DisclosureDirection)direction animate:(BOOL)animate {
  DisclosureDirection originalDirection = self.disclosureDirection;

  // Default trailing rotation is 0 (no rotation), rotate 180 degrees if RTL.
  float trailingRotation = 0;
  if (base::i18n::IsRTL()) {
    trailingRotation = (-180 / 180.0) * M_PI;
  }

  if (originalDirection != direction) {
    self.disclosureDirection = direction;
    CGFloat angle = direction == DisclosureDirectionDown ? kRotationNinetyCW
                                                         : trailingRotation;

    // Update the accessibility hint to match the new direction.
    self.accessibilityHint =
        direction == DisclosureDirectionDown
            ? l10n_util::GetNSString(
                  IDS_IOS_RECENT_TABS_DISCLOSURE_VIEW_EXPANDED_HINT)
            : l10n_util::GetNSString(
                  IDS_IOS_RECENT_TABS_DISCLOSURE_VIEW_COLLAPSED_HINT);

    if (animate) {
      __weak TableViewDisclosureHeaderFooterView* weakSelf = self;
      [self.cellAnimator addAnimations:^{
        weakSelf.disclosureImageView.transform =
            CGAffineTransformRotate(CGAffineTransformIdentity, angle);
      }];
    } else {
      self.disclosureImageView.transform =
          CGAffineTransformRotate(CGAffineTransformIdentity, angle);
    }
  }
}

- (UIColor*)cellDefaultBackgroundColor {
  if (!_cellDefaultBackgroundColor) {
    _cellDefaultBackgroundColor = self.contentView.backgroundColor
                                      ? self.contentView.backgroundColor
                                      : UIColor.clearColor;
  }
  return _cellDefaultBackgroundColor;
}

#pragma mark - Accessibility

- (NSString*)accessibilityLabel {
  // If no subtitleLabel text has been set only use the titleLabel text.
  if (![self.subtitleLabel.text length])
    return self.titleLabel.text;

  return [NSString stringWithFormat:@"%@, %@", self.titleLabel.text,
                                    self.subtitleLabel.text];
}

@end
