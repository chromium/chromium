// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/material_cell_catalog_view_controller.h"

#import <UIKit/UIKit.h>

#import "base/mac/foundation_util.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/grit/components_scaled_resources.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_configurator.h"
#import "ios/chrome/browser/ui/authentication/cells/signin_promo_view_delegate.h"
#import "ios/chrome/browser/ui/authentication/signin_promo_view_mediator.h"
#import "ios/chrome/browser/ui/autofill/cells/cvc_item.h"
#import "ios/chrome/browser/ui/autofill/cells/legacy_autofill_edit_item.h"
#import "ios/chrome/browser/ui/autofill/cells/status_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/MDCCollectionViewCell+Chrome.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_account_item.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_cell_style.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_footer_item.h"
#import "ios/chrome/browser/ui/collection_view/collection_view_model.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_footer_item.h"
#import "ios/chrome/browser/ui/content_suggestions/cells/content_suggestions_item.h"
#import "ios/chrome/browser/ui/icons/chrome_icon.h"
#import "ios/chrome/browser/ui/payments/cells/accepted_payment_methods_item.h"
#import "ios/chrome/browser/ui/payments/cells/autofill_profile_item.h"
#import "ios/chrome/browser/ui/payments/cells/payments_text_item.h"
#import "ios/chrome/browser/ui/payments/cells/price_item.h"
#import "ios/chrome/browser/ui/settings/cells/copied_to_chrome_item.h"
#import "ios/chrome/browser/ui/settings/cells/legacy/legacy_settings_detail_item.h"
#import "ios/chrome/browser/ui/settings/cells/passphrase_error_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_multiline_detail_item.h"
#import "ios/chrome/browser/ui/settings/cells/settings_text_item.h"
#import "ios/chrome/browser/ui/util/uikit_ui_util.h"
#import "ios/public/provider/chrome/browser/chrome_browser_provider.h"
#import "ios/public/provider/chrome/browser/signin/signin_resources_provider.h"
#import "ios/third_party/material_components_ios/src/components/CollectionCells/src/MaterialCollectionCells.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

typedef NS_ENUM(NSInteger, SectionIdentifier) {
  SectionIdentifierTextCell = kSectionIdentifierEnumZero,
  SectionIdentifierDetailCell,
  SectionIdentifierSwitchCell,
  SectionIdentifierNativeAppCell,
  SectionIdentifierAutofill,
  SectionIdentifierPayments,
  SectionIdentifierPaymentsNoBackground,
  SectionIdentifierCopiedToChrome,
  SectionIdentifierPasswordDetails,
  SectionIdentifierSettingsSearchCell,
  SectionIdentifierAccountCell,
  SectionIdentifierAccountControlCell,
  SectionIdentifierFooters,
  SectionIdentifierContentSuggestionsCell,
};

typedef NS_ENUM(NSInteger, ItemType) {
  ItemTypeTextCheckmark = kItemTypeEnumZero,
  ItemTypeTextDetail,
  ItemTypeText,
  ItemTypeDetailBasic,
  ItemTypeDetailLeftMedium,
  ItemTypeDetailRightMedium,
  ItemTypeDetailLeftLong,
  ItemTypeDetailRightLong,
  ItemTypeDetailBothLong,
  ItemTypeMultilineBasic,
  ItemTypeImportDataMultiline,
  ItemTypeSwitchDynamicHeight,
  ItemTypeHeader,
  ItemTypeAccountDetail,
  ItemTypeAccountCheckMark,
  ItemTypeAccountSignIn,
  ItemTypeApp,
  ItemTypePaymentsSingleLine,
  ItemTypePaymentsDynamicHeight,
  ItemTypeCopiedToChrome,
  ItemTypeSettingsSearch,
  ItemTypeAutofillDynamicHeight,
  ItemTypeAutofillCVC,
  ItemTypeAutofillStatus,
  ItemTypeFooter,
  ItemTypeSyncPassphraseError,
  ItemTypeContentSuggestions,
};

// Credit Card icon size.
const CGFloat kCardIssuerNetworkIconDimension = 25.0;

}  // namespace

@implementation MaterialCellCatalogViewController

- (instancetype)init {
  UICollectionViewLayout* layout = [[MDCCollectionViewFlowLayout alloc] init];
  self = [super initWithLayout:layout
                         style:CollectionViewControllerStyleDefault];
  if (self) {
    // TODO(crbug.com/764578): -loadModel should not be called from
    // initializer. A possible fix is to move this call to -viewDidLoad.
    [self loadModel];
  }
  return self;
}

- (void)loadModel {
  [super loadModel];
  CollectionViewModel* model = self.collectionViewModel;

  // Text cells.
  [model addSectionWithIdentifier:SectionIdentifierTextCell];

  SettingsTextItem* textHeader =
      [[SettingsTextItem alloc] initWithType:ItemTypeHeader];
  textHeader.text = @"SettingsTextCell";
  textHeader.textFont = [textHeader.textFont fontWithSize:12];
  textHeader.textColor = [[MDCPalette greyPalette] tint500];
  [model setHeader:textHeader
      forSectionWithIdentifier:SectionIdentifierTextCell];

  SettingsTextItem* textCell =
      [[SettingsTextItem alloc] initWithType:ItemTypeTextCheckmark];
  textCell.text = @"Text cell 1";
  textCell.accessoryType = MDCCollectionViewCellAccessoryCheckmark;
  [model addItem:textCell toSectionWithIdentifier:SectionIdentifierTextCell];
  SettingsTextItem* textCell2 =
      [[SettingsTextItem alloc] initWithType:ItemTypeTextDetail];
  textCell2.text =
      @"Text cell with text that is so long it must truncate at some point "
      @"except that maybe it sometimes doesn't in landscape mode.";
  textCell2.accessoryType = MDCCollectionViewCellAccessoryDetailButton;
  [model addItem:textCell2 toSectionWithIdentifier:SectionIdentifierTextCell];
  SettingsTextItem* smallTextCell =
      [[SettingsTextItem alloc] initWithType:ItemTypeText];
  smallTextCell.text = @"Text cell with small font but height of 48.";
  smallTextCell.textFont = [smallTextCell.textFont fontWithSize:8];
  [model addItem:smallTextCell
      toSectionWithIdentifier:SectionIdentifierTextCell];

  // Detail cells.
  [model addSectionWithIdentifier:SectionIdentifierDetailCell];
  LegacySettingsDetailItem* detailBasic =
      [[LegacySettingsDetailItem alloc] initWithType:ItemTypeDetailBasic];
  detailBasic.text = @"Preload Webpages";
  detailBasic.detailText = @"Only on Wi-Fi";
  detailBasic.accessoryType = MDCCollectionViewCellAccessoryDisclosureIndicator;
  [model addItem:detailBasic
      toSectionWithIdentifier:SectionIdentifierDetailCell];
  LegacySettingsDetailItem* detailMediumLeft =
      [[LegacySettingsDetailItem alloc] initWithType:ItemTypeDetailLeftMedium];
  detailMediumLeft.text = @"A long string but it should fit";
  detailMediumLeft.detailText = @"Detail";
  [model addItem:detailMediumLeft
      toSectionWithIdentifier:SectionIdentifierDetailCell];
  LegacySettingsDetailItem* detailMediumRight =
      [[LegacySettingsDetailItem alloc] initWithType:ItemTypeDetailRightMedium];
  detailMediumRight.text = @"Main";
  detailMediumRight.detailText = @"A long string but it should fit";
  [model addItem:detailMediumRight
      toSectionWithIdentifier:SectionIdentifierDetailCell];
  LegacySettingsDetailItem* detailLongLeft =
      [[LegacySettingsDetailItem alloc] initWithType:ItemTypeDetailLeftLong];
  detailLongLeft.text =
      @"This is a very long main text that is intended to overflow "
      @"except maybe on landscape but now it's longer so it won't fit.";
  detailLongLeft.detailText = @"Detail Text";
  detailLongLeft.iconImageName = @"ntp_history_icon";
  [model addItem:detailLongLeft
      toSectionWithIdentifier:SectionIdentifierDetailCell];
  LegacySettingsDetailItem* detailLongRight =
      [[LegacySettingsDetailItem alloc] initWithType:ItemTypeDetailRightLong];
  detailLongRight.text = @"Main Text";
  detailLongRight.detailText =
      @"This is a very long detail text that is intended to overflow "
      @"except maybe on landscape but now it's longer so it won't fit.";
  detailLongRight.iconImageName = @"ntp_history_icon";
  [model addItem:detailLongRight
      toSectionWithIdentifier:SectionIdentifierDetailCell];
  LegacySettingsDetailItem* detailLongBoth =
      [[LegacySettingsDetailItem alloc] initWithType:ItemTypeDetailBothLong];
  detailLongBoth.text =
      @"This is a very long main text that is intended to overflow "
      @"except maybe on landscape but now it's longer so it won't fit.";
  detailLongBoth.detailText =
      @"This is a very long detail text that is intended to overflow "
      @"except maybe on landscape but now it's longer so it won't fit.";
  detailLongBoth.iconImageName = @"ntp_history_icon";
  [model addItem:detailLongBoth
      toSectionWithIdentifier:SectionIdentifierDetailCell];

  // Autofill cells.
  [model addSectionWithIdentifier:SectionIdentifierAutofill];
  [model addItem:[self autofillEditItem]
      toSectionWithIdentifier:SectionIdentifierAutofill];
  [model addItem:[self autofillEditItemWithIcon]
      toSectionWithIdentifier:SectionIdentifierAutofill];
  [model addItem:[self cvcItem]
      toSectionWithIdentifier:SectionIdentifierAutofill];
  [model addItem:[self cvcItemWithDate]
      toSectionWithIdentifier:SectionIdentifierAutofill];
  [model addItem:[self cvcItemWithError]
      toSectionWithIdentifier:SectionIdentifierAutofill];
  [model addItem:[self statusItemVerifying]
      toSectionWithIdentifier:SectionIdentifierAutofill];
  [model addItem:[self statusItemVerified]
      toSectionWithIdentifier:SectionIdentifierAutofill];
  [model addItem:[self statusItemError]
      toSectionWithIdentifier:SectionIdentifierAutofill];

  // Payments cells.
  [model addSectionWithIdentifier:SectionIdentifierPayments];
  [model addItem:[self paymentsItemWithWrappingTextandOptionalImage]
      toSectionWithIdentifier:SectionIdentifierPayments];

  PriceItem* priceItem1 =
      [[PriceItem alloc] initWithType:ItemTypePaymentsSingleLine];
  priceItem1.item = @"Total";
  priceItem1.notification = @"Updated";
  priceItem1.price = @"USD $100.00";
  [model addItem:priceItem1 toSectionWithIdentifier:SectionIdentifierPayments];
  PriceItem* priceItem2 =
      [[PriceItem alloc] initWithType:ItemTypePaymentsSingleLine];
  priceItem2.item = @"Price label is long and should get clipped";
  priceItem2.notification = @"Updated";
  priceItem2.price = @"USD $1,000,000.00";
  [model addItem:priceItem2 toSectionWithIdentifier:SectionIdentifierPayments];
  PriceItem* priceItem3 =
      [[PriceItem alloc] initWithType:ItemTypePaymentsSingleLine];
  priceItem3.item = @"Price label is long and should get clipped";
  priceItem3.notification = @"Should get clipped too";
  priceItem3.price = @"USD $1,000,000.00";
  [model addItem:priceItem3 toSectionWithIdentifier:SectionIdentifierPayments];
  PriceItem* priceItem4 =
      [[PriceItem alloc] initWithType:ItemTypePaymentsSingleLine];
  priceItem4.item = @"Price label is long and should get clipped";
  priceItem4.notification = @"Should get clipped too";
  priceItem4.price = @"USD $1,000,000,000.00";
  [model addItem:priceItem4 toSectionWithIdentifier:SectionIdentifierPayments];

  AutofillProfileItem* profileItem1 =
      [[AutofillProfileItem alloc] initWithType:ItemTypePaymentsDynamicHeight];
  profileItem1.name = @"Profile Name gets wrapped if it's too long";
  profileItem1.address = @"Profile Address also gets wrapped if it's too long";
  profileItem1.phoneNumber = @"123-456-7890";
  profileItem1.email = @"foo@bar.com";
  profileItem1.notification = @"Some fields are missing";
  [model addItem:profileItem1
      toSectionWithIdentifier:SectionIdentifierPayments];
  AutofillProfileItem* profileItem2 =
      [[AutofillProfileItem alloc] initWithType:ItemTypePaymentsDynamicHeight];
  profileItem1.name = @"All fields are optional";
  profileItem2.phoneNumber = @"123-456-7890";
  profileItem2.notification = @"Some fields are missing";
  [model addItem:profileItem2
      toSectionWithIdentifier:SectionIdentifierPayments];
  AutofillProfileItem* profileItem3 =
      [[AutofillProfileItem alloc] initWithType:ItemTypePaymentsDynamicHeight];
  profileItem3.address = @"All fields are optional";
  profileItem3.email = @"foo@bar.com";
  [model addItem:profileItem3
      toSectionWithIdentifier:SectionIdentifierPayments];

  // Payments cells with no background.
  [model addSectionWithIdentifier:SectionIdentifierPaymentsNoBackground];
  [model addItem:[self acceptedPaymentMethodsItem]
      toSectionWithIdentifier:SectionIdentifierPaymentsNoBackground];

  // CopiedToChrome cells.
  [model addSectionWithIdentifier:SectionIdentifierCopiedToChrome];
  CopiedToChromeItem* copiedToChromeItem =
      [[CopiedToChromeItem alloc] initWithType:ItemTypeCopiedToChrome];
  [model addItem:copiedToChromeItem
      toSectionWithIdentifier:SectionIdentifierCopiedToChrome];

  // Account cells.
  [model addSectionWithIdentifier:SectionIdentifierAccountCell];
  [model addItem:[self accountItemDetailWithError]
      toSectionWithIdentifier:SectionIdentifierAccountCell];
  [model addItem:[self accountItemCheckMark]
      toSectionWithIdentifier:SectionIdentifierAccountCell];

  // Content Suggestions cells.
  [model addSectionWithIdentifier:SectionIdentifierContentSuggestionsCell];
  [model addItem:[self contentSuggestionsItem]
      toSectionWithIdentifier:SectionIdentifierContentSuggestionsCell];
  [model addItem:[self contentSuggestionsFooterItem]
      toSectionWithIdentifier:SectionIdentifierContentSuggestionsCell];

  // Footers.
  [model addSectionWithIdentifier:SectionIdentifierFooters];
  [model addItem:[self shortFooterItem]
      toSectionWithIdentifier:SectionIdentifierFooters];
  [model addItem:[self longFooterItem]
      toSectionWithIdentifier:SectionIdentifierFooters];
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.title = @"Collection Cell Catalog";
}

#pragma mark MDCCollectionViewStylingDelegate

- (CGFloat)collectionView:(nonnull UICollectionView*)collectionView
    cellHeightAtIndexPath:(nonnull NSIndexPath*)indexPath {
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  switch (item.type) {
    case ItemTypeContentSuggestions:
    case ItemTypeFooter:
    case ItemTypeSwitchDynamicHeight:
    case ItemTypeTextCheckmark:
    case ItemTypeTextDetail:
    case ItemTypeText:
    case ItemTypeMultilineBasic:
    case ItemTypeImportDataMultiline:
    case ItemTypeAutofillCVC:
    case ItemTypeAutofillStatus:
    case ItemTypePaymentsDynamicHeight:
    case ItemTypeAutofillDynamicHeight:
      return [MDCCollectionViewCell
          cr_preferredHeightForWidth:CGRectGetWidth(collectionView.bounds)
                             forItem:item];
    case ItemTypeApp:
      return MDCCellDefaultOneLineWithAvatarHeight;
    case ItemTypeAccountDetail:
      return MDCCellDefaultTwoLineHeight;
    case ItemTypeAccountCheckMark:
      return MDCCellDefaultTwoLineHeight;
    case ItemTypeAccountSignIn:
      return MDCCellDefaultThreeLineHeight;
    default:
      return MDCCellDefaultOneLineHeight;
  }
}

- (MDCCollectionViewCellStyle)collectionView:(UICollectionView*)collectionView
                         cellStyleForSection:(NSInteger)section {
  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:section];
  switch (sectionIdentifier) {
    case SectionIdentifierFooters:
      // Display the Learn More footer in the default style with no "card" UI
      // and no section padding.
      return MDCCollectionViewCellStyleDefault;
    default:
      return self.styler.cellStyle;
  }
}

- (BOOL)collectionView:(UICollectionView*)collectionView
    shouldHideItemBackgroundAtIndexPath:(NSIndexPath*)indexPath {
  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:indexPath.section];
  switch (sectionIdentifier) {
    case SectionIdentifierPaymentsNoBackground:
    case SectionIdentifierFooters:
      // Display the Learn More footer without any background image or
      // shadowing.
      return YES;
    default:
      return NO;
  }
}

- (BOOL)collectionView:(nonnull UICollectionView*)collectionView
    hidesInkViewAtIndexPath:(nonnull NSIndexPath*)indexPath {
  NSInteger sectionIdentifier =
      [self.collectionViewModel sectionIdentifierForSection:indexPath.section];
  if (sectionIdentifier == SectionIdentifierFooters)
    return YES;
  CollectionViewItem* item =
      [self.collectionViewModel itemAtIndexPath:indexPath];
  switch (item.type) {
    case ItemTypeApp:
    case ItemTypeSwitchDynamicHeight:
      return YES;
    default:
      return NO;
  }
}

#pragma mark Item models

- (CollectionViewItem*)accountItemDetailWithError {
  CollectionViewAccountItem* accountItemDetail =
      [[CollectionViewAccountItem alloc] initWithType:ItemTypeAccountDetail];
  accountItemDetail.cellStyle = CollectionViewCellStyle::kUIKit;
  // TODO(crbug.com/754032): ios_default_avatar image is from a downstream iOS
  // internal repository. It should be used through a provider API instead.
  accountItemDetail.image = [UIImage imageNamed:@"ios_default_avatar"];
  accountItemDetail.text = @"Account User Name";
  accountItemDetail.detailText =
      @"Syncing to AccountUserNameAccount@example.com";
  accountItemDetail.accessoryType =
      MDCCollectionViewCellAccessoryDisclosureIndicator;
  accountItemDetail.shouldDisplayError = YES;
  return accountItemDetail;
}

- (CollectionViewItem*)accountItemCheckMark {
  CollectionViewAccountItem* accountItemCheckMark =
      [[CollectionViewAccountItem alloc] initWithType:ItemTypeAccountCheckMark];
  accountItemCheckMark.cellStyle = CollectionViewCellStyle::kUIKit;
  // TODO(crbug.com/754032): ios_default_avatar image is from a downstream iOS
  // internal repository. It should be used through a provider API instead.
  accountItemCheckMark.image = [UIImage imageNamed:@"ios_default_avatar"];
  accountItemCheckMark.text = @"Lorem ipsum dolor sit amet, consectetur "
                              @"adipiscing elit, sed do eiusmod tempor "
                              @"incididunt ut labore et dolore magna aliqua.";
  accountItemCheckMark.detailText =
      @"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
      @"eiusmod tempor incididunt ut labore et dolore magna aliqua.";
  accountItemCheckMark.accessoryType = MDCCollectionViewCellAccessoryCheckmark;
  return accountItemCheckMark;
}

#pragma mark Private

- (CollectionViewItem*)paymentsItemWithWrappingTextandOptionalImage {
  PaymentsTextItem* item =
      [[PaymentsTextItem alloc] initWithType:ItemTypePaymentsDynamicHeight];
  item.text = @"If you want to display a long text that wraps to the next line "
              @"and may need to feature an image this is the cell to use.";
  item.leadingImage = [UIImage imageNamed:@"app_icon_placeholder"];
  return item;
}

- (CollectionViewItem*)acceptedPaymentMethodsItem {
  AcceptedPaymentMethodsItem* item = [[AcceptedPaymentMethodsItem alloc]
      initWithType:ItemTypePaymentsDynamicHeight];
  item.message = @"Cards accepted:";

  NSMutableArray* cardTypeIcons = [NSMutableArray array];
  const char* cardTypes[]{autofill::kVisaCard,
                          autofill::kMasterCard,
                          autofill::kAmericanExpressCard,
                          autofill::kJCBCard,
                          autofill::kDinersCard,
                          autofill::kDiscoverCard};
  for (const std::string& cardType : cardTypes) {
    autofill::data_util::PaymentRequestData data =
        autofill::data_util::GetPaymentRequestData(cardType);
    UIImage* cardTypeIcon =
        ResizeImage(NativeImage(data.icon_resource_id),
                    CGSizeMake(kCardIssuerNetworkIconDimension,
                               kCardIssuerNetworkIconDimension),
                    ProjectionMode::kAspectFillNoClipping);
    [cardTypeIcons addObject:cardTypeIcon];
  }
  item.methodTypeIcons = cardTypeIcons;
  return item;
}

- (CollectionViewItem*)autofillEditItem {
  LegacyAutofillEditItem* item = [[LegacyAutofillEditItem alloc]
      initWithType:ItemTypeAutofillDynamicHeight];
  item.cellStyle = CollectionViewCellStyle::kUIKit;
  item.textFieldName = @"Required Card Number";
  item.textFieldValue = @"4111111111111111";
  item.textFieldEnabled = YES;
  item.required = YES;
  return item;
}

- (CollectionViewItem*)autofillEditItemWithIcon {
  LegacyAutofillEditItem* item = [[LegacyAutofillEditItem alloc]
      initWithType:ItemTypeAutofillDynamicHeight];
  item.cellStyle = CollectionViewCellStyle::kUIKit;
  item.textFieldName = @"Card Number";
  item.textFieldValue = @"4111111111111111";
  item.textFieldEnabled = YES;
  int resourceID =
      autofill::data_util::GetPaymentRequestData(autofill::kVisaCard)
          .icon_resource_id;
  item.identifyingIcon =
      ResizeImage(NativeImage(resourceID), CGSizeMake(30.0, 30.0),
                  ProjectionMode::kAspectFillNoClipping);
  return item;
}

- (CollectionViewItem*)cvcItem {
  CVCItem* item = [[CVCItem alloc] initWithType:ItemTypeAutofillCVC];
  item.instructionsText =
      @"This is a long text explaining to enter card details and what "
      @"will happen afterwards.";
  item.CVCImageResourceID = IDR_CREDIT_CARD_CVC_HINT;
  return item;
}

- (CollectionViewItem*)cvcItemWithDate {
  CVCItem* item = [[CVCItem alloc] initWithType:ItemTypeAutofillCVC];
  item.instructionsText =
      @"This is a long text explaining to enter card details and what "
      @"will happen afterwards.";
  item.CVCImageResourceID = IDR_CREDIT_CARD_CVC_HINT;
  item.showDateInput = YES;
  return item;
}

- (CollectionViewItem*)cvcItemWithError {
  CVCItem* item = [[CVCItem alloc] initWithType:ItemTypeAutofillCVC];
  item.instructionsText =
      @"This is a long text explaining to enter card details and what "
      @"will happen afterwards. Is this long enough to span 3 lines?";
  item.errorMessage = @"Some error";
  item.CVCImageResourceID = IDR_CREDIT_CARD_CVC_HINT_AMEX;
  item.showNewCardButton = YES;
  item.showCVCInputError = YES;
  return item;
}

- (CollectionViewItem*)statusItemVerifying {
  StatusItem* item = [[StatusItem alloc] initWithType:ItemTypeAutofillStatus];
  item.text = @"Verifying…";
  return item;
}

- (CollectionViewItem*)statusItemVerified {
  StatusItem* item = [[StatusItem alloc] initWithType:ItemTypeAutofillStatus];
  item.state = StatusItemState::VERIFIED;
  item.text = @"Verified!";
  return item;
}

- (CollectionViewItem*)statusItemError {
  StatusItem* item = [[StatusItem alloc] initWithType:ItemTypeAutofillStatus];
  item.state = StatusItemState::ERROR;
  item.text = @"There was a really long error. We can't tell you more, but we "
              @"will still display this long string.";
  return item;
}

- (CollectionViewFooterItem*)shortFooterItem {
  CollectionViewFooterItem* footerItem =
      [[CollectionViewFooterItem alloc] initWithType:ItemTypeFooter];
  footerItem.text = @"Hello";
  return footerItem;
}

- (CollectionViewFooterItem*)longFooterItem {
  CollectionViewFooterItem* footerItem =
      [[CollectionViewFooterItem alloc] initWithType:ItemTypeFooter];
  footerItem.text = @"Hello Hello Hello Hello Hello Hello Hello Hello Hello "
                    @"Hello Hello Hello Hello Hello Hello Hello Hello Hello "
                    @"Hello Hello Hello Hello Hello Hello Hello Hello Hello ";
  footerItem.image = [UIImage imageNamed:@"app_icon_placeholder"];
  return footerItem;
}

- (ContentSuggestionsItem*)contentSuggestionsItem {
  ContentSuggestionsItem* articleItem = [[ContentSuggestionsItem alloc]
      initWithType:ItemTypeContentSuggestions
             title:@"This is an incredible article, you should read it!"
               url:GURL()];
  articleItem.publisher = @"Top Publisher.com";
  return articleItem;
}

- (ContentSuggestionsFooterItem*)contentSuggestionsFooterItem {
  ContentSuggestionsFooterItem* footerItem =
      [[ContentSuggestionsFooterItem alloc]
          initWithType:ItemTypeContentSuggestions
                 title:@"Footer title"
              callback:nil];
  return footerItem;
}

@end
