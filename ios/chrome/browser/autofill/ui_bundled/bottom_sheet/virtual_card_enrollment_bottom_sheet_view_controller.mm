// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/autofill/ui_bundled/bottom_sheet/virtual_card_enrollment_bottom_sheet_view_controller.h"

#import "build/branding_buildflags.h"
#import "components/autofill/core/browser/payments/payments_service_url.h"
#import "components/autofill/core/common/autofill_payments_features.h"
#import "components/grit/components_scaled_resources.h"
#import "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/net/model/crurl.h"
#import "ios/chrome/browser/shared/ui/symbols/symbols.h"
#import "ios/chrome/browser/shared/ui/table_view/cells/table_view_detail_icon_item.h"
#import "ios/chrome/browser/shared/ui/util/uikit_ui_util.h"
#import "ios/chrome/common/ui/colors/semantic_color_names.h"
#import "ios/chrome/common/ui/confirmation_alert/confirmation_alert_action_handler.h"
#import "ios/chrome/common/ui/util/text_view_util.h"
#import "ui/base/l10n/l10n_util.h"
#import "url/gurl.h"

static NSString* kDetailIconCellIdentifier = @"DetailIconCell";

namespace {

// The padding above and below the logo image.
CGFloat const kLogoPadding = 16;

// The padding above and below the illustration image.
CGFloat const kIllustrationPadding = 20;

// The spacing between vertically stacked elements.
CGFloat const kVerticalSpacingMedium = 16;

// The credit card corner radius.
CGFloat const kCreditCardCellCornerRadius = 10;

// The credit card cell height.
CGFloat const kCreditCardCellHeight = 64;

}  // namespace

@interface VirtualCardEnrollmentBottomSheetViewController () <
    UITableViewDataSource,
    UITableViewDelegate,
    UITextViewDelegate,
    ConfirmationAlertActionHandler>
@end

@implementation VirtualCardEnrollmentBottomSheetViewController {
  VirtualCardEnrollmentBottomSheetData* _bottomSheetData;

  UITextView* _explanatoryMessageView;
  UIStackView* _customUnderTitleView;
}

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.actionHandler = self;

  // Prevent extra spacing at the top of the bottom sheet content.
  self.topAlignedLayout = YES;

  // Set the spacing between the two stack views.
  self.customSpacing = kVerticalSpacingMedium;

  // Remove extra space between the scroll view bottom and last legal message.
  self.customScrollViewBottomInsets = 0;

  // Hide the "Done" button in the navigation bar.
  self.showDismissBarButton = NO;

  self.aboveTitleView = [self createAboveTitleStackView];

  _customUnderTitleView = [self createUnderTitleView];
  [self addLegalMessages:_bottomSheetData.paymentServerLegalMessageLines];
  [self addLegalMessages:_bottomSheetData.issuerLegalMessageLines];

  self.underTitleView = _customUnderTitleView;

  [super viewDidLoad];
}

#pragma mark - VirtualCardEnrollmentBottomSheetConsumer

- (void)setCardData:(VirtualCardEnrollmentBottomSheetData*)data {
  _bottomSheetData = data;
  self.primaryActionString = data.acceptActionText;
  self.secondaryActionString = data.cancelActionText;
}

- (void)showLoadingState {
  self.primaryActionButton.accessibilityLabel = l10n_util::GetNSString(
      IDS_AUTOFILL_VIRTUAL_CARD_ENROLL_LOADING_THROBBER_ACCESSIBLE_NAME);
  self.isLoading = YES;
  self.isConfirmed = NO;
}

- (void)showConfirmationState {
  self.isLoading = NO;
  self.isConfirmed = YES;
  UIAccessibilityPostNotification(
      UIAccessibilityAnnouncementNotification,
      l10n_util::GetNSString(
          IDS_AUTOFILL_VIRTUAL_CARD_ENROLLED_ACCESSIBILITY_ANNOUNCEMENT));
}

#pragma mark - ConfirmationAlertActionHandler

- (void)confirmationAlertPrimaryAction {
  // Accept button was clicked.
  [self.mutator didAccept];
}

- (void)confirmationAlertSecondaryAction {
  // Dismiss button was clicked.
  [self.mutator didCancel];
}

#pragma mark - UITableViewDataSource

- (NSInteger)tableView:(UITableView*)tableView
    numberOfRowsInSection:(NSInteger)section {
  return 1;
}

- (NSInteger)numberOfSectionsInTableView:(UITableView*)tableView {
  return 1;
}

- (UITableViewCell*)tableView:(UITableView*)tableView
        cellForRowAtIndexPath:(NSIndexPath*)indexPath {
  TableViewDetailIconCell* cell =
      [tableView dequeueReusableCellWithIdentifier:kDetailIconCellIdentifier];

  cell.selectionStyle = UITableViewCellSelectionStyleNone;
  cell.backgroundColor = [UIColor colorNamed:kSecondaryBackgroundColor];
  cell.userInteractionEnabled = NO;
  cell.accessibilityIdentifier =
      _bottomSheetData.creditCard.cardNameAndLastFourDigits;
  [cell.textLabel
      setText:_bottomSheetData.creditCard.cardNameAndLastFourDigits];
  [cell setDetailText:_bottomSheetData.creditCard.cardDetails];
  [cell setIconImage:_bottomSheetData.creditCard.icon
            tintColor:nil
      backgroundColor:cell.backgroundColor
         cornerRadius:kCreditCardCellCornerRadius];
  [cell setTextLayoutConstraintAxis:UILayoutConstraintAxisVertical];

  return cell;
}

#pragma mark - Private

// Create the view containing the logo, illustration, title and message.
- (UIStackView*)createAboveTitleStackView {
  UIStackView* aboveTitleStackView =
      [[UIStackView alloc] initWithFrame:CGRectZero];
  aboveTitleStackView.layoutMarginsRelativeArrangement = YES;
  aboveTitleStackView.axis = UILayoutConstraintAxisVertical;
  aboveTitleStackView.spacing = kVerticalSpacingMedium;

  [aboveTitleStackView
      addArrangedSubview:[[UIView alloc]
                             initWithFrame:CGRectMake(0, 0, 0, kLogoPadding)]];

  [aboveTitleStackView addArrangedSubview:[self createGooglePayLogoView]];
  CGFloat logoIllustrationSpacerHeight =
      kLogoPadding + kIllustrationPadding - aboveTitleStackView.spacing;
  [aboveTitleStackView
      addArrangedSubview:
          [self createVerticalSpacerView:logoIllustrationSpacerHeight]];
  [aboveTitleStackView addArrangedSubview:[self createIllustrationView]];
  [aboveTitleStackView
      addArrangedSubview:[self createVerticalSpacerView:kIllustrationPadding]];
  [aboveTitleStackView addArrangedSubview:[self createTitleLabel]];
  _explanatoryMessageView = [self createExplanatoryMessageTextView];
  [aboveTitleStackView addArrangedSubview:_explanatoryMessageView];
  return aboveTitleStackView;
}

- (UIImageView*)createGooglePayLogoView {
  UIImageView* logoImageTitleView =
      [[UIImageView alloc] initWithImage:[self googlePayBadgeImage]];
  logoImageTitleView.contentMode = UIViewContentModeCenter;
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableVcnEnrollLoadingAndConfirmation)) {
    logoImageTitleView.isAccessibilityElement = YES;
    logoImageTitleView.accessibilityLabel =
        l10n_util::GetNSString(IDS_AUTOFILL_GOOGLE_PAY_LOGO_ACCESSIBLE_NAME);
  }
#endif
  return logoImageTitleView;
}

- (UIView*)createVerticalSpacerView:(CGFloat)height {
  return [[UIView alloc] initWithFrame:CGRectMake(0, 0, 0, height)];
}

// Returns the google pay badge image corresponding to the current
// UIUserInterfaceStyle (light/dark mode).
- (UIImage*)googlePayBadgeImage {
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  return MakeSymbolMulticolor(CustomSymbolWithPointSize(
      kGooglePaySymbol, kCreditCardCellHeight - 2 * kLogoPadding));
#else
  return NativeImage(IDR_AUTOFILL_GOOGLE_PAY);
#endif
}

- (UIImageView*)createIllustrationView {
  UIImageView* illustrationImageView = [[UIImageView alloc]
      initWithImage:[UIImage
                        imageNamed:@"virtual_card_enrollment_illustration"]];
  illustrationImageView.contentMode = UIViewContentModeCenter;
  return illustrationImageView;
}

- (UILabel*)createTitleLabel {
  UILabel* titleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
  titleLabel.accessibilityTraits |= UIAccessibilityTraitHeader;
  titleLabel.text = _bottomSheetData.title;
  titleLabel.numberOfLines = 0;  // Allow multiple lines.
  UIFontDescriptor* title2FontDescriptor =
      [UIFont preferredFontForTextStyle:UIFontTextStyleTitle2].fontDescriptor;
  titleLabel.font = [UIFont
      fontWithDescriptor:[title2FontDescriptor fontDescriptorWithSymbolicTraits:
                                                   UIFontDescriptorTraitBold]
                    size:0];
  titleLabel.textColor = [UIColor colorNamed:kTextPrimaryColor];
  titleLabel.textAlignment = NSTextAlignmentCenter;
  return titleLabel;
}

- (UITextView*)createExplanatoryMessageTextView {
  UITextView* explanatoryMessageView = CreateUITextViewWithTextKit1();
  explanatoryMessageView.scrollEnabled = NO;
  explanatoryMessageView.editable = NO;
  explanatoryMessageView.delegate = self;
  explanatoryMessageView.translatesAutoresizingMaskIntoConstraints = NO;
  explanatoryMessageView.textContainerInset = UIEdgeInsetsZero;
  explanatoryMessageView.linkTextAttributes =
      @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]};
  explanatoryMessageView.attributedText =
      [self attributedTextForExplanatoryMessage];
  return explanatoryMessageView;
}

- (NSAttributedString*)attributedTextForExplanatoryMessage {
  NSRange rangeOfLearnMore = [_bottomSheetData.explanatoryMessage
      rangeOfString:_bottomSheetData.learnMoreLinkText];
  NSMutableParagraphStyle* centeredTextStyle =
      [[NSMutableParagraphStyle alloc] init];
  centeredTextStyle.alignment = NSTextAlignmentCenter;
  NSMutableAttributedString* attributedText = [[NSMutableAttributedString alloc]
      initWithString:_bottomSheetData.explanatoryMessage
          attributes:@{
            NSForegroundColorAttributeName :
                [UIColor colorNamed:kTextPrimaryColor],
            NSFontAttributeName :
                [UIFont preferredFontForTextStyle:UIFontTextStyleFootnote],
            NSParagraphStyleAttributeName : centeredTextStyle,
          }];
  [attributedText addAttribute:NSLinkAttributeName
                         value:@"unused"
                         range:rangeOfLearnMore];
  return attributedText;
}

- (UIStackView*)createUnderTitleView {
  UIStackView* underTitleView = [[UIStackView alloc] initWithFrame:CGRectZero];
  underTitleView.axis = UILayoutConstraintAxisVertical;
  underTitleView.spacing = kVerticalSpacingMedium;

  [underTitleView addArrangedSubview:[self createCardContainerTableView]];
  return underTitleView;
}

- (UITableView*)createCardContainerTableView {
  UITableView* cardContainerTable =
      [[UITableView alloc] initWithFrame:CGRectZero];
  cardContainerTable.rowHeight = kCreditCardCellHeight;
  cardContainerTable.separatorStyle = UITableViewCellSeparatorStyleNone;
  cardContainerTable.layer.cornerRadius = kCreditCardCellCornerRadius;
  [cardContainerTable registerClass:[TableViewDetailIconCell class]
             forCellReuseIdentifier:kDetailIconCellIdentifier];
  cardContainerTable.dataSource = self;
  cardContainerTable.delegate = self;
  [cardContainerTable.heightAnchor
      constraintEqualToConstant:kCreditCardCellHeight]
      .active = YES;
  return cardContainerTable;
}

// Adds a text view for the given legal message to the under title view.
- (void)addLegalMessages:(NSArray<SaveCardMessageWithLinks*>*)messages {
  for (SaveCardMessageWithLinks* message in messages) {
    UITextView* textView = CreateUITextViewWithTextKit1();
    textView.scrollEnabled = NO;
    textView.editable = NO;
    textView.delegate = self;
    textView.translatesAutoresizingMaskIntoConstraints = NO;
    textView.textContainerInset = UIEdgeInsetsZero;
    textView.linkTextAttributes =
        @{NSForegroundColorAttributeName : [UIColor colorNamed:kBlueColor]};
    textView.backgroundColor = UIColor.clearColor;
    textView.attributedText = [VirtualCardEnrollmentBottomSheetViewController
        attributedTextForText:message.messageText
                     linkUrls:message.linkURLs
                   linkRanges:message.linkRanges];
    [_customUnderTitleView addArrangedSubview:textView];
  }
}

+ (NSAttributedString*)attributedTextForText:(NSString*)text
                                    linkUrls:(std::vector<GURL>)linkURLs
                                  linkRanges:(NSArray*)linkRanges {
  NSMutableParagraphStyle* centeredTextStyle =
      [[NSMutableParagraphStyle alloc] init];
  centeredTextStyle.alignment = NSTextAlignmentCenter;
  NSDictionary* textAttributes = @{
    NSFontAttributeName :
        [UIFont preferredFontForTextStyle:UIFontTextStyleCaption2],
    NSForegroundColorAttributeName : [UIColor colorNamed:kTextSecondaryColor],
    NSParagraphStyleAttributeName : centeredTextStyle,
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
  return attributedText;
}

#pragma mark - UITextViewDelegate

- (BOOL)textView:(UITextView*)textView
    shouldInteractWithURL:(NSURL*)URL
                  inRange:(NSRange)characterRange
              interaction:(UITextItemInteraction)interaction {
  if (textView == _explanatoryMessageView) {
    // The learn more link was clicked.
    [self.delegate
        didTapLinkURL:[[CrURL alloc]
                          initWithGURL:autofill::payments::
                                           GetVirtualCardEnrollmentSupportUrl()]
                 text:[textView.text substringWithRange:characterRange]];
    return NO;
  } else {
    // A link in a legal message was clicked.
    [self.delegate
        didTapLinkURL:[[CrURL alloc] initWithNSURL:URL]
                 text:[textView.text substringWithRange:characterRange]];
    return NO;
  }
}

- (UIAction*)textView:(UITextView*)textView
    primaryActionForTextItem:(UITextItem*)textItem
               defaultAction:(UIAction*)defaultAction API_AVAILABLE(ios(17.0)) {
  CrURL* URL = nil;
  if (textView == _explanatoryMessageView) {
    URL = [[CrURL alloc]
        initWithGURL:autofill::payments::GetVirtualCardEnrollmentSupportUrl()];
  } else {
    URL = [[CrURL alloc] initWithNSURL:textItem.link];
  }
  __weak __typeof__(self) weakSelf = self;
  return [UIAction actionWithHandler:^(UIAction* action) {
    [weakSelf.delegate
        didTapLinkURL:URL
                 text:[textView.text substringWithRange:textItem.range]];
  }];
}

@end
