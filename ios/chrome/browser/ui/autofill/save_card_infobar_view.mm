// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/autofill/save_card_infobar_view.h"

#include "base/logging.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/procedural_block_types.h"
#import "ios/chrome/browser/ui/autofill/save_card_infobar_view_delegate.h"
#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#import "ios/chrome/browser/ui/infobars/infobar_constants.h"
#import "ios/chrome/browser/ui/infobars/infobar_view_sizing_delegate.h"
#import "ios/chrome/browser/ui/util/label_link_controller.h"
#import "ios/chrome/browser/ui/util/named_guide.h"
#include "ios/chrome/browser/ui/util/ui_util.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/Buttons/src/MaterialButtons.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"
#include "ui/base/l10n/l10n_util.h"
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

// Color in RGB to be used as tint color on the icon.
const CGFloat kIconTintColor = 0x1A73E8;

// Padding used on the bottom edge of the title.
const CGFloat kTitleBottomPadding = 24;

// Vertical spacing between the views.
const CGFloat kVerticalSpacing = 16;

// Horizontal spacing between the views.
const CGFloat kHorizontalSpacing = 8;

// Padding used on the top edge of the action buttons' container.
const CGFloat kButtonsTopPadding = 32;

// Color in RGB used as background of the action buttons.
const int kButtonTitleColorBlue = 0x4285f4;

// Corner radius for action buttons.
const CGFloat kButtonCornerRadius = 8.0;

// Returns the font for the infobar message.
UIFont* InfoBarMessageFont() {
  return [UIFont preferredFontForTextStyle:UIFontTextStyleBody];
}

}  // namespace

@implementation MessageWithLinks

@synthesize messageText = _messageText;
@synthesize linkRanges = _linkRanges;
@synthesize linkURLs = _linkURLs;

@end

@interface SaveCardInfoBarView ()

// Allows styled and clickable links in the message label. May be nil.
@property(nonatomic, strong) LabelLinkController* messageLinkController;

// Allows styled and clickable links in the legal message label. May be nil.
@property(nonatomic, strong) LabelLinkController* legalMessageLinkController;

// Constraint used to add bottom margin to the view.
@property(nonatomic) NSLayoutConstraint* footerViewBottomAnchorConstraint;

// Creates and adds subviews.
- (void)setupSubviews;

// Returns the view containing the infobar title. No padding is required on the
// view. If |googlePayIcon| is present, returns a view containing the GooglePay
// icon followed by the title laid out horizontally.
- (UIView*)titleView;

// Returns the view containing the infobar content. No padding is required on
// the view.
- (UIView*)contentView;

// Creates and returns an instance of LabelLinkController for |label| and
// |action| which is invoked when links managed by it are tapped.
- (LabelLinkController*)labelLinkControllerWithLabel:(UILabel*)label
                                              action:(ProceduralBlockWithURL)
                                                         action;

// Creates and returns an infobar action button initialized with the given
// title, colors, and action.
- (UIButton*)actionButtonWithTitle:(NSString*)title
                           palette:(MDCPalette*)palette
                        titleColor:(UIColor*)titleColor
                            target:(id)target
                            action:(SEL)action;

// Handles tapping the close button.
- (void)didTapClose;

// Handles tapping the cancel button.
- (void)didTapCancel;

// Handles tapping the confirm button.
- (void)didTapConfirm;

@end

@implementation SaveCardInfoBarView

@synthesize visibleHeight = _visibleHeight;
@synthesize sizingDelegate = _sizingDelegate;
@synthesize delegate = _delegate;
@synthesize icon = _icon;
@synthesize googlePayIcon = _googlePayIcon;
@synthesize message = _message;
@synthesize closeButtonImage = _closeButtonImage;
@synthesize description = _description;
@synthesize cardIssuerIcon = _cardIssuerIcon;
@synthesize cardLabel = _cardLabel;
@synthesize cardSublabel = _cardSublabel;
@synthesize legalMessages = _legalMessages;
@synthesize cancelButtonTitle = _cancelButtonTitle;
@synthesize confirmButtonTitle = _confirmButtonTitle;
@synthesize messageLinkController = _messageLinkController;
@synthesize legalMessageLinkController = _legalMessageLinkController;
@synthesize footerViewBottomAnchorConstraint =
    _footerViewBottomAnchorConstraint;

#pragma mark - UIView

- (void)willMoveToSuperview:(UIView*)newSuperview {
  // Create and add subviews the first time this moves to a superview.
  if (newSuperview && !self.subviews.count) {
    [self setupSubviews];
  }
}

- (void)layoutSubviews {
  // Set a bottom margin equal to the height of the secondary toolbar, if any.
  // Deduct the bottom safe area inset as it is already included in the height
  // of the secondary toolbar.
  // TODO(crbug.com/894449): This won't update the infobar's position after the
  // secondary toolbar reappears. Consider adding a constraint to the
  // |layoutGuide| in |didMoveToSuperview|.
  NamedGuide* layoutGuide =
      [NamedGuide guideWithName:kSecondaryToolbarGuide view:self];
  self.footerViewBottomAnchorConstraint.constant =
      layoutGuide.layoutFrame.size.height;

  [super layoutSubviews];

  [self.sizingDelegate didSetInfoBarTargetHeight:CGRectGetHeight(self.frame)];
}

- (CGSize)sizeThatFits:(CGSize)size {
  CGSize computedSize = [self systemLayoutSizeFittingSize:size];
  return CGSizeMake(size.width, computedSize.height);
}

#pragma mark - Helper methods

- (void)setupSubviews {
  [self setAccessibilityViewIsModal:YES];
  if (IsUIRefreshPhase1Enabled()) {
    self.backgroundColor = UIColorFromRGB(kInfobarBackgroundColor);
  } else {
    self.backgroundColor = [UIColor whiteColor];
  }
  id<LayoutGuideProvider> safeAreaLayoutGuide =
      SafeAreaLayoutGuideForView(self);

  // Add the icon. The icon is fixed to the top leading corner of the infobar.
  // |iconContainerView| is used here because the AutoLayout constraints for
  // UIImageView would get ignored otherwise.
  // TODO(crbug.com/850288): Investigate why this is happening.
  UIView* iconContainerView = nil;
  if (self.icon) {
    iconContainerView = [[UIView alloc] initWithFrame:CGRectZero];
    iconContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    iconContainerView.clipsToBounds = YES;
    UIImageView* iconImageView = [[UIImageView alloc] initWithImage:self.icon];
    iconImageView.tintColor = UIColorFromRGB(kIconTintColor);
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
  closeButton.tintColor = [UIColor blackColor];
  closeButton.alpha = 0.20;
  // Prevent the button from shrinking or expanding horizontally.
  [closeButton
      setContentCompressionResistancePriority:UILayoutPriorityRequired
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [closeButton setContentHuggingPriority:UILayoutPriorityRequired
                                 forAxis:UILayoutConstraintAxisHorizontal];
  [headerView addSubview:closeButton];
  [NSLayoutConstraint activateConstraints:@[
    [closeButton.leadingAnchor
        constraintEqualToAnchor:titleView.trailingAnchor],
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
    footerView.translatesAutoresizingMaskIntoConstraints = NO;
    footerView.clipsToBounds = YES;
    footerView.axis = UILayoutConstraintAxisHorizontal;
    footerView.spacing = kHorizontalSpacing;
    footerView.layoutMarginsRelativeArrangement = YES;
    footerView.layoutMargins =
        UIEdgeInsetsMake(kButtonsTopPadding, kPadding, kPadding, kPadding);
    [self addSubview:footerView];

    self.footerViewBottomAnchorConstraint =
        [self.bottomAnchor constraintEqualToAnchor:footerView.bottomAnchor];
    [NSLayoutConstraint activateConstraints:@[
      [safeAreaLayoutGuide.leadingAnchor
          constraintEqualToAnchor:footerView.leadingAnchor],
      [safeAreaLayoutGuide.trailingAnchor
          constraintEqualToAnchor:footerView.trailingAnchor],
      [contentView.bottomAnchor constraintEqualToAnchor:footerView.topAnchor],
      self.footerViewBottomAnchorConstraint
    ]];

    // Dummy view that expands so that the action buttons are aligned to the
    // trailing edge of the |footerView|.
    UIView* dummyView = [[UIView alloc] initWithFrame:CGRectZero];
    [dummyView
        setContentCompressionResistancePriority:UILayoutPriorityFittingSizeLevel
                                        forAxis:
                                            UILayoutConstraintAxisHorizontal];
    [footerView addArrangedSubview:dummyView];

    if (self.cancelButtonTitle.length > 0UL) {
      UIButton* cancelButton =
          [self actionButtonWithTitle:self.cancelButtonTitle
                              palette:nil
                           titleColor:UIColorFromRGB(kButtonTitleColorBlue)
                               target:self
                               action:@selector(didTapCancel)];

      [footerView addArrangedSubview:cancelButton];
    }

    if (self.confirmButtonTitle.length > 0UL) {
      UIButton* confirmButton =
          [self actionButtonWithTitle:self.confirmButtonTitle
                              palette:[MDCPalette cr_bluePalette]
                           titleColor:[UIColor whiteColor]
                               target:self
                               action:@selector(didTapConfirm)];

      [footerView addArrangedSubview:confirmButton];
    }
  } else {
    [NSLayoutConstraint activateConstraints:@[
      [contentView.bottomAnchor
          constraintEqualToAnchor:safeAreaLayoutGuide.bottomAnchor]
    ]];
  }
}

- (UIView*)titleView {
  UIView* titleView = [[UIView alloc] initWithFrame:CGRectZero];
  titleView.translatesAutoresizingMaskIntoConstraints = NO;
  titleView.clipsToBounds = YES;

  // |iconContainerView| is used here because the AutoLayout constraints for
  // UIImageView would get ignored otherwise.
  // TODO(crbug.com/850288): Investigate why this is happening.
  UIView* iconContainerView = nil;
  if (self.googlePayIcon) {
    iconContainerView = [[UIView alloc] initWithFrame:CGRectZero];
    iconContainerView.translatesAutoresizingMaskIntoConstraints = NO;
    iconContainerView.clipsToBounds = YES;
    UIImageView* iconImageView =
        [[UIImageView alloc] initWithImage:self.googlePayIcon];
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
  label.translatesAutoresizingMaskIntoConstraints = NO;
  label.clipsToBounds = YES;
  label.textColor = [[MDCPalette greyPalette] tint900];
  label.numberOfLines = 0;
  label.backgroundColor = [UIColor clearColor];
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

- (UIView*)contentView {
  UIStackView* contentView = [[UIStackView alloc] initWithArrangedSubviews:@[]];
  contentView.translatesAutoresizingMaskIntoConstraints = NO;
  contentView.clipsToBounds = YES;
  contentView.axis = UILayoutConstraintAxisVertical;
  contentView.spacing = kVerticalSpacing;

  // Description.
  if (self.description.length > 0UL) {
    UILabel* label = [[UILabel alloc] initWithFrame:CGRectZero];
    label.translatesAutoresizingMaskIntoConstraints = NO;
    label.textColor = [[MDCPalette greyPalette] tint700];
    label.numberOfLines = 0;
    label.backgroundColor = [UIColor clearColor];
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
  DCHECK_GT(self.cardSublabel.length, 0UL);

  // The leading edge aligned card details container view. Contains the card
  // issuer network icon, the card label, and the card sublabel.
  UIStackView* cardDetailsContainerView =
      [[UIStackView alloc] initWithArrangedSubviews:@[]];
  cardDetailsContainerView.translatesAutoresizingMaskIntoConstraints = NO;
  cardDetailsContainerView.clipsToBounds = YES;
  cardDetailsContainerView.axis = UILayoutConstraintAxisHorizontal;
  cardDetailsContainerView.spacing = kHorizontalSpacing;
  [contentView addArrangedSubview:cardDetailsContainerView];

  UIImageView* cardIssuerIconImageView =
      [[UIImageView alloc] initWithImage:self.cardIssuerIcon];
  [cardDetailsContainerView addArrangedSubview:cardIssuerIconImageView];

  UILabel* cardLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  cardLabel.translatesAutoresizingMaskIntoConstraints = NO;
  cardLabel.text = self.cardLabel;
  cardLabel.font = [MDCTypography body1Font];
  cardLabel.textColor = [[MDCPalette greyPalette] tint900];
  cardLabel.backgroundColor = [UIColor clearColor];
  [cardDetailsContainerView addArrangedSubview:cardLabel];

  UILabel* cardSublabel = [[UILabel alloc] initWithFrame:CGRectZero];
  cardSublabel.translatesAutoresizingMaskIntoConstraints = NO;
  cardSublabel.text = self.cardSublabel;
  cardSublabel.font = [MDCTypography body1Font];
  cardSublabel.textColor = [[MDCPalette greyPalette] tint700];
  cardSublabel.backgroundColor = [UIColor clearColor];
  [cardDetailsContainerView addArrangedSubview:cardSublabel];

  // Dummy view that expands so that the card details are aligned to the leading
  // edge of the |contentView|.
  UIView* dummyView = [[UIView alloc] initWithFrame:CGRectZero];
  [dummyView
      setContentCompressionResistancePriority:UILayoutPriorityFittingSizeLevel
                                      forAxis:UILayoutConstraintAxisHorizontal];
  [cardDetailsContainerView addArrangedSubview:dummyView];

  // Legal messages.
  auto block = ^(MessageWithLinks* legalMessage, NSUInteger idx, BOOL* stop) {
    DCHECK_GT(legalMessage.messageText.length, 0UL);
    DCHECK_EQ(legalMessage.linkURLs.size(), legalMessage.linkRanges.count);

    UILabel* label = [[UILabel alloc] initWithFrame:CGRectZero];
    label.translatesAutoresizingMaskIntoConstraints = NO;
    label.textColor = [[MDCPalette greyPalette] tint700];
    label.numberOfLines = 0;
    label.backgroundColor = [UIColor clearColor];
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

- (LabelLinkController*)labelLinkControllerWithLabel:(UILabel*)label
                                              action:(ProceduralBlockWithURL)
                                                         action {
  LabelLinkController* labelLinkController =
      [[LabelLinkController alloc] initWithLabel:label action:action];
  [labelLinkController setLinkColor:[[MDCPalette cr_bluePalette] tint500]];
  return labelLinkController;
}

- (UIButton*)actionButtonWithTitle:(NSString*)title
                           palette:(MDCPalette*)palette
                        titleColor:(UIColor*)titleColor
                            target:(id)target
                            action:(SEL)action {
  MDCFlatButton* button = [[MDCFlatButton alloc] init];
  button.translatesAutoresizingMaskIntoConstraints = NO;
  button.titleLabel.adjustsFontSizeToFitWidth = YES;
  button.titleLabel.minimumScaleFactor = 0.6f;
  [button setTitle:title forState:UIControlStateNormal];
  [button addTarget:target
                action:action
      forControlEvents:UIControlEventTouchUpInside];
  [button setUnderlyingColorHint:[UIColor blackColor]];
  button.uppercaseTitle = NO;
  button.layer.cornerRadius = kButtonCornerRadius;
  [button
      setTitleFont:[UIFont preferredFontForTextStyle:UIFontTextStyleHeadline]
          forState:UIControlStateNormal];

  if (palette) {
    button.hasOpaqueBackground = YES;
    button.inkColor = [[palette tint300] colorWithAlphaComponent:0.5f];
    [button setBackgroundColor:[palette tint500] forState:UIControlStateNormal];
  }

  if (titleColor) {
    button.tintAdjustmentMode = UIViewTintAdjustmentModeNormal;
    button.tintColor = titleColor;
    [button setTitleColor:titleColor forState:UIControlStateNormal];
  }

  return button;
}

- (void)didTapClose {
  [self.delegate saveCardInfoBarViewDidTapClose:self];
}

- (void)didTapCancel {
  [self.delegate saveCardInfoBarViewDidTapCancel:self];
}

- (void)didTapConfirm {
  [self.delegate saveCardInfoBarViewDidTapConfirm:self];
}

@end
