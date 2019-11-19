// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/save_card_infobar_view.h"

#include "base/logging.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/procedural_block_types.h"
#import "ios/chrome/browser/ui/autofill/save_card_infobar_view_delegate.h"
#import "ios/chrome/browser/ui/autofill/save_card_message_with_links.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/infobars/infobar_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_constants.h"
#import "ios/chrome/browser/ui/util/label_link_controller.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/colors/semantic_color_names.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util.h"
#import "ui/gfx/ios/uikit_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Padding used on the edges of the infobar.
const CGFloat kPadding = 16;

// Line height for the title message.
const CGFloat kTitleLineHeight = 24;

// Line height for the description and legal messages.
const CGFloat kDescriptionLineHeight = 20;

// Padding used on the close button to increase the touchable area. This added
// to the close button icon's intrinsic padding equals kPadding.
const CGFloat kCloseButtonPadding = 12;

// Padding used on the bottom edge of the title.
const CGFloat kTitleBottomPadding = 24;

// Vertical spacing between the views.
const CGFloat kVerticalSpacing = 16;

// Horizontal spacing between the views.
const CGFloat kHorizontalSpacing = 8;

// Padding used on the top edge of the action buttons' container.
const CGFloat kButtonsTopPadding = 32;

// Corner radius for action buttons.
const CGFloat kButtonCornerRadius = 8.0;

// Returns the font for the infobar message.
UIFont* InfoBarMessageFont() {
  return [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
}

NSString* const kActionButtonsDummyViewAccessibilityIdentifier =
    @"actionButtonsDummyView";
NSString* const kCardDetailsContainerViewAccessibilityIdentifier =
    @"cardDetailsContainerView";
NSString* const kCardDetailsDummyViewAccessibilityIdentifier =
    @"cardDetailsDummyView";
NSString* const kCardIssuerIconImageViewAccessibilityIdentifier =
    @"cardIssuerIconImageView";
NSString* const kCardLabelAccessibilityIdentifier = @"cardLabel";
NSString* const kCardSublabelAccessibilityIdentifier = @"cardSublabel";
NSString* const kContentViewAccessibilityIdentifier = @"contentView";
NSString* const kDescriptionLabelAccessibilityIdentifier = @"descriptionLabel";
NSString* const kFooterViewAccessibilityIdentifier = @"footerView";
NSString* const kGPayIconContainerViewAccessibilityIdentifier =
    @"gPayIconContainerView";
NSString* const kGPayIconImageViewAccessibilityIdentifier =
    @"gPayIconImageView";
NSString* const kHeaderViewAccessibilityIdentifier = @"headerView";
NSString* const kIconContainerViewAccessibilityIdentifier =
    @"iconContainerView";
NSString* const kIconImageViewAccessibilityIdentifier = @"iconImageView";
NSString* const kLegalMessageLabelAccessibilityIdentifier =
    @"legalMessageLabel";
NSString* const kTitleLabelAccessibilityIdentifier = @"titleLabel";
NSString* const kTitleViewAccessibilityIdentifier = @"titleView";

}  // namespace

@interface SaveCardInfoBarView ()

// Allows styled and clickable links in the message label. May be nil.
@property(nonatomic, strong) LabelLinkController* messageLinkController;

// Allows styled and clickable links in the legal message label. May be nil.
@property(nonatomic, strong) LabelLinkController* legalMessageLinkController;

// Constraint used to add bottom margin to the view.
@property(nonatomic) NSLayoutConstraint* bottomAnchorConstraint;

// How much of the infobar (in points) is visible (e.g., during showing/hiding
// animation).
@property(nonatomic, assign) CGFloat visibleHeight;

@end

@implementation SaveCardInfoBarView

// Synthesize description because the existing NSObject description property
// is readonly.
@synthesize description = _description;

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  // Create and add subviews the first time this moves to a superview.
  if (newSuperview && !self.subviews.count) {
    [self setupSubviews];
    // Lower constraint's priority to avoid breaking other constraints while
    // |newSuperview| is animating.
    // TODO(crbug.com/904521): Investigate why this is needed.
    self.bottomAnchorConstraint.priority = UILayoutPriorityDefaultLow;
  }
  [super willMoveToSuperview:newSuperview];
}

- (void)didMoveToSuperview {
  [super didMoveToSuperview];
  if (!self.superview)
    return;

  // Increase constraint's priority after the view was added to its superview.
  // TODO(crbug.com/904521): Investigate why this is needed.
  self.bottomAnchorConstraint.priority = UILayoutPriorityDefaultHigh;
}

- (CGSize)sizeThatFits:(CGSize)size {
  // Calculate the safe area and current Toolbar height. Set the
  // bottomAnchorConstraint constant to this height to create the bottom
  // padding.
  CGFloat bottomSafeAreaInset = self.safeAreaInsets.bottom;
  CGFloat toolbarHeight = 0;
  UILayoutGuide* guide =
      [NamedGuide guideWithName:kSecondaryToolbarGuide view:self];
  UILayoutGuide* guideNoFullscreen =
      [NamedGuide guideWithName:kSecondaryToolbarNoFullscreenGuide view:self];
  if (guide && guideNoFullscreen) {
    CGFloat toolbarHeightCurrent = guide.layoutFrame.size.height;
    CGFloat toolbarHeightMax = guideNoFullscreen.layoutFrame.size.height;
    if (toolbarHeightMax > 0) {
      CGFloat fullscreenProgress = toolbarHeightCurrent / toolbarHeightMax;
      CGFloat toolbarHeightInSafeArea = toolbarHeightMax - bottomSafeAreaInset;
      toolbarHeight += fullscreenProgress * toolbarHeightInSafeArea;
    }
  }
  self.bottomAnchorConstraint.constant = toolbarHeight + bottomSafeAreaInset;

  // Now that the constraint constant has been set calculate the fitting size.
  CGSize computedSize = [self systemLayoutSizeFittingSize:size];
  return CGSizeMake(size.width, computedSize.height);
}

#pragma mark - Helper methods

// Creates and adds subviews.
- (void)setupSubviews {
  [self setAccessibilityViewIsModal:YES];
  self.backgroundColor = [UIColor colorNamed:kBackgroundColor];
  id<LayoutGuideProvider> safeAreaLayoutGuide = self.safeAreaLayoutGuide;

  // Add thin separator to separate infobar from the web content.
  UIView* separator = [[UIView alloc] initWithFrame:CGRectZero];
  separator.translatesAutoresizingMaskIntoConstraints = NO;
  separator.backgroundColor = [UIColor colorNamed:kToolbarShadowColor];
  [self addSubview:separator];
  CGFloat separatorHeight = ui::AlignValueToUpperPixel(kToolbarSeparatorHeight);
  [NSLayoutConstraint activateConstraints:@[
    [separator.heightAnchor constraintEqualToConstant:separatorHeight],
    [self.leadingAnchor constraintEqualToAnchor:separator.leadingAnchor],
    [self.trailingAnchor constraintEqualToAnchor:separator.trailingAnchor],
    [self.topAnchor constraintEqualToAnchor:separator.bottomAnchor],
  ]];

  // Add the icon. The icon is fixed to the top leading corner of the infobar.
  // |iconContainerView| is used here because the AutoLayout constraints for
  // UIImageView would get ignored otherwise.
  // TODO(crbug.com/850288): Investigate why this is happening.
  UIView* iconContainerView = nil;
  if (self.icon) {
    iconContainerView = [[UIView alloc] initWithFrame:CGRectZero];
    iconContainerView.accessibilityIdentifier =
        kIconContainerViewAccessibilityIdentifier;
    iconContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    iconContainerView.clipsToBounds = YES;
    UIImageView* iconImageView = [[UIImageView alloc] initWithImage:self.icon];
    iconImageView.translatesAutoresizingMaskIntoConstraints = NO;
    iconImageView.accessibilityIdentifier =
        kIconImageViewAccessibilityIdentifier;
    // Prevent the icon from shrinking horizontally. This is needed when the
    // title is long and needs to wrap.
    [iconImageView
        setContentCompressionResistancePriority:UILayoutPriorityRequired
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [iconContainerView addSubview:iconImageView];
    AddSameConstraints(iconContainerView, iconImageView);
    [self addSubview:iconContainerView];
    [NSLayoutConstraint activateConstraints:@[
      [iconContainerView.leadingAnchor
          constraintEqualToAnchor:safeAreaLayoutGuide.leadingAnchor
                         constant:kPadding],
      [iconContainerView.topAnchor
          constraintEqualToAnchor:safeAreaLayoutGuide.topAnchor
                         constant:kPadding],
    ]];
  }

  // The container view that lays out the title and close button horizontally.
  // +---------------------------+
  // || ICON | TITLE        | X ||
  // ||      |-------------------|
  // ||      |                  ||
  // |---------------------------|
  // ||  FOOTER                 ||
  // +---------------------------+
  UIView* headerView = [[UIView alloc] initWithFrame:CGRectZero];
  headerView.accessibilityIdentifier = kHeaderViewAccessibilityIdentifier;
  headerView.translatesAutoresizingMaskIntoConstraints = NO;
  headerView.clipsToBounds = YES;
  [self addSubview:headerView];
  NSLayoutConstraint* headerViewLeadingConstraint =
      self.icon ? [headerView.leadingAnchor
                      constraintEqualToAnchor:iconContainerView.trailingAnchor
                                     constant:kHorizontalSpacing]
                : [headerView.leadingAnchor
                      constraintEqualToAnchor:safeAreaLayoutGuide.leadingAnchor
                                     constant:kPadding];
  // Note that kPadding is not used on the trailing edge of the header view.
  // |closeButton| provides the required padding thorugh its contentEdgeInsets
  // while keeping the trailing edge touchable.
  [NSLayoutConstraint activateConstraints:@[
    headerViewLeadingConstraint,
    [headerView.topAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.topAnchor],
    [headerView.trailingAnchor
        constraintEqualToAnchor:safeAreaLayoutGuide.trailingAnchor],
  ]];

  // Add the title view.
  UIView* titleView = [self titleView];
  [headerView addSubview:titleView];
  [NSLayoutConstraint activateConstraints:@[
    [titleView.leadingAnchor constraintEqualToAnchor:headerView.leadingAnchor],
    [titleView.topAnchor constraintEqualToAnchor:headerView.topAnchor
                                        constant:kPadding],
    [titleView.bottomAnchor constraintEqualToAnchor:headerView.bottomAnchor
                                           constant:-kTitleBottomPadding],
  ]];

  // Add the close button. The close button is fixed to the trailing edge of the
  // infobar since it cannot expand.
  DCHECK(self.closeButtonImage);
  UIButton* closeButton = [UIButton buttonWithType:UIButtonTypeSystem];
  closeButton.translatesAutoresizingMaskIntoConstraints = NO;
  [closeButton setImage:self.closeButtonImage forState:UIControlStateNormal];
  closeButton.contentEdgeInsets =
      UIEdgeInsetsMake(kCloseButtonPadding, kCloseButtonPadding,
                       kCloseButtonPadding, kCloseButtonPadding);
  [closeButton addTarget:self
                  action:@selector(didTapClose)
        forControlEvents:UIControlEventTouchUpInside];
  [closeButton setAccessibilityLabel:l10n_util::GetNSString(IDS_CLOSE)];
  closeButton.tintColor = [UIColor colorNamed:kToolbarButtonColor];
  // Prevent the button from shrinking horizontally. This is needed when the
  // title is long and needs to wrap.
  [closeButton
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [headerView addSubview:closeButton];
  [NSLayoutConstraint activateConstraints:@[
    [closeButton.leadingAnchor
        constraintGreaterThanOrEqualToAnchor:titleView.trailingAnchor],
    [closeButton.topAnchor constraintEqualToAnchor:headerView.topAnchor],
    [closeButton.trailingAnchor
        constraintEqualToAnchor:headerView.trailingAnchor],
  ]];

  // Add the content view.
  UIView* contentView = [self contentView];
  [self addSubview:contentView];
  [NSLayoutConstraint activateConstraints:@[
    [contentView.leadingAnchor
        constraintEqualToAnchor:headerView.leadingAnchor],
    [contentView.topAnchor constraintEqualToAnchor:headerView.bottomAnchor],
    [contentView.trailingAnchor
        constraintEqualToAnchor:headerView.trailingAnchor
                       constant:-kPadding],
  ]];

  // The container view that lays out the action buttons horizontally.
  // +---------------------------+
  // || ICON | TITLE        | X ||
  // ||      |-------------------|
  // ||      |                  ||
  // |---------------------------|
  // ||           | CANCEL | OK ||
  // +---------------------------+
  if (self.cancelButtonTitle.length > 0UL ||
      self.confirmButtonTitle.length > 0UL) {
    UIStackView* footerView =
        [[UIStackView alloc] initWithArrangedSubviews:@[]];
    footerView.accessibilityIdentifier = kFooterViewAccessibilityIdentifier;
    footerView.translatesAutoresizingMaskIntoConstraints = NO;
    footerView.clipsToBounds = YES;
    footerView.axis = UILayoutConstraintAxisHorizontal;
    footerView.spacing = kHorizontalSpacing;
    footerView.layoutMarginsRelativeArrangement = YES;
    footerView.layoutMargins =
        UIEdgeInsetsMake(kButtonsTopPadding, kPadding, kPadding, kPadding);
    [self addSubview:footerView];

    self.bottomAnchorConstraint =
        [self.bottomAnchor constraintEqualToAnchor:footerView.bottomAnchor];
    [NSLayoutConstraint activateConstraints:@[
      [safeAreaLayoutGuide.leadingAnchor
          constraintEqualToAnchor:footerView.leadingAnchor],
      [safeAreaLayoutGuide.trailingAnchor
          constraintEqualToAnchor:footerView.trailingAnchor],
      [contentView.bottomAnchor constraintEqualToAnchor:footerView.topAnchor],
      self.bottomAnchorConstraint
    ]];

    // Dummy view that expands so that the action buttons are aligned to the
    // trailing edge of the |footerView|.
    UIView* dummyView = [[UIView alloc] initWithFrame:CGRectZero];
    dummyView.accessibilityIdentifier =
        kActionButtonsDummyViewAccessibilityIdentifier;
    [footerView addArrangedSubview:dummyView];

    if (self.cancelButtonTitle.length > 0UL) {
      UIButton* cancelButton =
          [self actionButtonWithTitle:self.cancelButtonTitle
                      backgroundColor:nil
                           titleColor:[UIColor colorNamed:kBlueColor]
                               target:self
                               action:@selector(didTapCancel)];

      [footerView addArrangedSubview:cancelButton];
    }

    if (self.confirmButtonTitle.length > 0UL) {
      UIButton* confirmButton =
          [self actionButtonWithTitle:self.confirmButtonTitle
                      backgroundColor:[UIColor colorNamed:kBlueColor]
                           titleColor:[UIColor colorNamed:kSolidButtonTextColor]
                               target:self
                               action:@selector(didTapConfirm)];

      [footerView addArrangedSubview:confirmButton];
    }
  } else {
    self.bottomAnchorConstraint =
        [self.bottomAnchor constraintEqualToAnchor:contentView.bottomAnchor];
    self.bottomAnchorConstraint.active = YES;
  }
}

// Returns the view containing the infobar title. No padding is required on the
// view. If |googlePayIcon| is present, returns a view containing the GooglePay
// icon followed by the title laid out horizontally.
- (UIView*)titleView {
  UIView* titleView = [[UIView alloc] initWithFrame:CGRectZero];
  titleView.accessibilityIdentifier = kTitleViewAccessibilityIdentifier;
  titleView.translatesAutoresizingMaskIntoConstraints = NO;
  titleView.clipsToBounds = YES;

  // |iconContainerView| is used here because the AutoLayout constraints for
  // UIImageView would get ignored otherwise.
  // TODO(crbug.com/850288): Investigate why this is happening.
  UIView* iconContainerView = nil;
  if (self.googlePayIcon) {
    iconContainerView = [[UIView alloc] initWithFrame:CGRectZero];
    iconContainerView.accessibilityIdentifier =
        kGPayIconContainerViewAccessibilityIdentifier;
    iconContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    iconContainerView.clipsToBounds = YES;
    UIImageView* iconImageView =
        [[UIImageView alloc] initWithImage:self.googlePayIcon];
    iconImageView.accessibilityIdentifier =
        kGPayIconImageViewAccessibilityIdentifier;
    iconImageView.translatesAutoresizingMaskIntoConstraints = NO;
    [iconContainerView addSubview:iconImageView];
    AddSameConstraints(iconContainerView, iconImageView);
    [titleView addSubview:iconContainerView];
    [NSLayoutConstraint activateConstraints:@[
      [iconContainerView.leadingAnchor
          constraintEqualToAnchor:titleView.leadingAnchor],
      [iconContainerView.centerYAnchor
          constraintEqualToAnchor:titleView.centerYAnchor],
    ]];
  }

  DCHECK_GT(self.message.messageText.length, 0UL);
  DCHECK_EQ(self.message.linkURLs.size(), self.message.linkRanges.count);

  UILabel* label = [[UILabel alloc] initWithFrame:CGRectZero];
  label.accessibilityIdentifier = kTitleLabelAccessibilityIdentifier;
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.clipsToBounds = YES;
  label.textColor = [UIColor colorNamed:kTextPrimaryColor];
  label.numberOfLines = 0;
  NSMutableParagraphStyle* paragraphStyle =
      [[NSMutableParagraphStyle alloc] init];
  paragraphStyle.lineBreakMode = NSLineBreakByWordWrapping;

  // Set the line height and vertically center the text.
  UIFont* font = InfoBarMessageFont();
  paragraphStyle.lineSpacing =
      (kTitleLineHeight - (font.ascender - font.descender)) / 2;
  paragraphStyle.minimumLineHeight =
      kTitleLineHeight - paragraphStyle.lineSpacing;
  paragraphStyle.maximumLineHeight = paragraphStyle.minimumLineHeight;

  NSDictionary* attributes = @{
    NSParagraphStyleAttributeName : paragraphStyle,
    NSFontAttributeName : InfoBarMessageFont(),
  };
  [label setAttributedText:[[NSAttributedString alloc]
                               initWithString:self.message.messageText
                                   attributes:attributes]];

  if (self.message.linkRanges.count > 0UL) {
    __weak SaveCardInfoBarView* weakSelf = self;
    self.messageLinkController = [self
        labelLinkControllerWithLabel:label
                              action:^(const GURL& URL) {
                                [weakSelf.delegate
                                    saveCardInfoBarViewDidTapLink:weakSelf];
                              }];

    auto block = ^(NSValue* rangeValue, NSUInteger idx, BOOL* stop) {
      [self.messageLinkController addLinkWithRange:rangeValue.rangeValue
                                               url:self.message.linkURLs[idx]];
    };
    [self.message.linkRanges enumerateObjectsUsingBlock:block];
  }

  [titleView addSubview:label];
  NSLayoutConstraint* labelLeadingConstraint =
      self.googlePayIcon
          ? [label.leadingAnchor
                constraintEqualToAnchor:iconContainerView.trailingAnchor
                               constant:kHorizontalSpacing]
          : [label.leadingAnchor
                constraintEqualToAnchor:titleView.leadingAnchor];
  [NSLayoutConstraint activateConstraints:@[
    labelLeadingConstraint,
    [label.topAnchor constraintEqualToAnchor:titleView.topAnchor],
    [label.trailingAnchor constraintEqualToAnchor:titleView.trailingAnchor],
    [label.bottomAnchor constraintEqualToAnchor:titleView.bottomAnchor],
  ]];

  return titleView;
}

// Returns the view containing the infobar content. No padding is required on
// the view.
- (UIView*)contentView {
  UIStackView* contentView = [[UIStackView alloc] initWithArrangedSubviews:@[]];
  contentView.accessibilityIdentifier = kContentViewAccessibilityIdentifier;
  contentView.translatesAutoresizingMaskIntoConstraints = NO;
  contentView.clipsToBounds = YES;
  contentView.axis = UILayoutConstraintAxisVertical;
  contentView.spacing = kVerticalSpacing;

  // Description.
  if (self.description.length > 0UL) {
    UILabel* label = [[UILabel alloc] initWithFrame:CGRectZero];
    label.accessibilityIdentifier = kDescriptionLabelAccessibilityIdentifier;
    label.textColor = [UIColor colorNamed:kTextSecondaryColor];
    label.numberOfLines = 0;
    NSMutableParagraphStyle* paragraphStyle =
        [[NSMutableParagraphStyle alloc] init];
    paragraphStyle.lineBreakMode = NSLineBreakByWordWrapping;
    paragraphStyle.minimumLineHeight = kDescriptionLineHeight;
    paragraphStyle.maximumLineHeight = paragraphStyle.minimumLineHeight;
    NSDictionary* attributes = @{
      NSParagraphStyleAttributeName : paragraphStyle,
      NSFontAttributeName : [MDCTypography body1Font],
    };
    [label setAttributedText:[[NSAttributedString alloc]
                                 initWithString:self.description
                                     attributes:attributes]];

    [contentView addArrangedSubview:label];
  }

  DCHECK(self.cardIssuerIcon);
  DCHECK_GT(self.cardLabel.length, 0UL);

  // The leading edge aligned card details container view. Contains the card
  // issuer network icon, the card label, and the card sublabel.
  UIStackView* cardDetailsContainerView =
      [[UIStackView alloc] initWithArrangedSubviews:@[]];
  cardDetailsContainerView.accessibilityIdentifier =
      kCardDetailsContainerViewAccessibilityIdentifier;
  cardDetailsContainerView.clipsToBounds = YES;
  cardDetailsContainerView.axis = UILayoutConstraintAxisHorizontal;
  cardDetailsContainerView.spacing = kHorizontalSpacing;
  [contentView addArrangedSubview:cardDetailsContainerView];

  UIImageView* cardIssuerIconImageView =
      [[UIImageView alloc] initWithImage:self.cardIssuerIcon];
  cardIssuerIconImageView.accessibilityIdentifier =
      kCardIssuerIconImageViewAccessibilityIdentifier;
  [cardDetailsContainerView addArrangedSubview:cardIssuerIconImageView];

  UILabel* cardLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  cardLabel.accessibilityIdentifier = kCardLabelAccessibilityIdentifier;
  cardLabel.text = self.cardLabel;
  cardLabel.font = [MDCTypography body1Font];
  cardLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  [cardDetailsContainerView addArrangedSubview:cardLabel];

  UILabel* cardSublabel = [[UILabel alloc] initWithFrame:CGRectZero];
  cardSublabel.accessibilityIdentifier = kCardSublabelAccessibilityIdentifier;
  cardSublabel.text = self.cardSublabel;
  cardSublabel.font = [MDCTypography body1Font];
  cardSublabel.textColor = [UIColor colorNamed:kTextSecondaryColor];
  [cardDetailsContainerView addArrangedSubview:cardSublabel];

  // Dummy view that expands so that the card details are aligned to the leading
  // edge of the |contentView|.
  UIView* dummyView = [[UIView alloc] initWithFrame:CGRectZero];
  dummyView.accessibilityIdentifier =
      kCardDetailsDummyViewAccessibilityIdentifier;
  [cardDetailsContainerView addArrangedSubview:dummyView];

  // Legal messages.
  auto block = ^(SaveCardMessageWithLinks* legalMessage, NSUInteger idx,
                 BOOL* stop) {
    DCHECK_GT(legalMessage.messageText.length, 0UL);
    DCHECK_EQ(legalMessage.linkURLs.size(), legalMessage.linkRanges.count);

    UILabel* label = [[UILabel alloc] initWithFrame:CGRectZero];
    label.accessibilityIdentifier = kLegalMessageLabelAccessibilityIdentifier;
    label.textColor = [[MDCPalette greyPalette] tint700];
    label.numberOfLines = 0;
    NSMutableParagraphStyle* paragraphStyle =
        [[NSMutableParagraphStyle alloc] init];
    paragraphStyle.lineBreakMode = NSLineBreakByWordWrapping;
    paragraphStyle.minimumLineHeight = kDescriptionLineHeight;
    paragraphStyle.maximumLineHeight = paragraphStyle.minimumLineHeight;
    NSDictionary* attributes = @{
      NSParagraphStyleAttributeName : paragraphStyle,
      NSFontAttributeName : [MDCTypography body1Font],
    };
    [label setAttributedText:[[NSAttributedString alloc]
                                 initWithString:legalMessage.messageText
                                     attributes:attributes]];

    if (legalMessage.linkRanges.count > 0UL) {
      __weak SaveCardInfoBarView* weakSelf = self;
      self.legalMessageLinkController =
          [self labelLinkControllerWithLabel:label
                                      action:^(const GURL& URL) {
                                        [weakSelf.delegate
                                            saveCardInfoBarView:weakSelf
                                             didTapLegalLinkURL:URL];
                                      }];
    }
    [legalMessage.linkRanges
        enumerateObjectsUsingBlock:^(NSValue* rangeValue, NSUInteger idx,
                                     BOOL* stop) {
          [self.legalMessageLinkController
              addLinkWithRange:rangeValue.rangeValue
                           url:legalMessage.linkURLs[idx]];
        }];
    [contentView addArrangedSubview:label];
  };
  [self.legalMessages enumerateObjectsUsingBlock:block];

  return contentView;
}

// Creates and returns an instance of LabelLinkController for |label| and
// |action| which is invoked when links managed by it are tapped.
- (LabelLinkController*)labelLinkControllerWithLabel:(UILabel*)label
                                              action:(ProceduralBlockWithURL)
                                                         action {
  LabelLinkController* labelLinkController =
      [[LabelLinkController alloc] initWithLabel:label action:action];
  [labelLinkController setLinkColor:[UIColor colorNamed:kBlueColor]];
  return labelLinkController;
}

// Creates and returns an infobar action button initialized with the given
// title, colors, and action.
- (UIButton*)actionButtonWithTitle:(NSString*)title
                   backgroundColor:(UIColor*)backgroundColor
                        titleColor:(UIColor*)titleColor
                            target:(id)target
                            action:(SEL)action {
  MDCFlatButton* button = [[MDCFlatButton alloc] init];
  button.titleLabel.adjustsFontSizeToFitWidth = YES;
  button.titleLabel.minimumScaleFactor = 0.6f;
  [button setTitle:title forState:UIControlStateNormal];
  [button addTarget:target
                action:action
      forControlEvents:UIControlEventTouchUpInside];
  [button setUnderlyingColorHint:[UIColor colorNamed:kBackgroundColor]];
  button.uppercaseTitle = NO;
  button.layer.cornerRadius = kButtonCornerRadius;
  [button
      setTitleFont:[UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]
          forState:UIControlStateNormal];

  if (backgroundColor) {
    button.hasOpaqueBackground = YES;
    button.inkColor = [UIColor colorNamed:kMDCInkColor];
    [button setBackgroundColor:backgroundColor forState:UIControlStateNormal];
  }

  if (titleColor) {
    button.tintAdjustmentMode = UIViewTintAdjustmentModeNormal;
    button.tintColor = titleColor;
    [button setTitleColor:titleColor forState:UIControlStateNormal];
  }

  return button;
}

// Handles tapping the close button.
- (void)didTapClose {
  [self.delegate saveCardInfoBarViewDidTapClose:self];
}

// Handles tapping the cancel button.
- (void)didTapCancel {
  [self.delegate saveCardInfoBarViewDidTapCancel:self];
}

// Handles tapping the confirm button.
- (void)didTapConfirm {
  [self.delegate saveCardInfoBarViewDidTapConfirm:self];
}

@end
